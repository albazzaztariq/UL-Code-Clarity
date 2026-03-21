#include "core/optimizer.h"
#include "core/jsonloader.h"

#include <QRegularExpression>
#include <QSet>
#include <QJsonArray>
#include <QJsonObject>

// ============================================================================
// JSON text loading helpers
// ============================================================================

static QJsonObject s_pythonPatterns;
static QJsonObject s_cppPatterns;
static QJsonObject s_crossLangNote;
static bool s_patternsLoaded = false;

static void loadOptimizerData()
{
    if (s_patternsLoaded) return;
    s_patternsLoaded = true;

    QJsonObject root = JsonLoader::loadObject("patterns_optimizer.json");

    // Index python patterns by id
    for (const QJsonValue& v : root["python"].toArray()) {
        QJsonObject o = v.toObject();
        s_pythonPatterns[o["id"].toString()] = o;
    }
    // Index cpp patterns by id
    for (const QJsonValue& v : root["cpp"].toArray()) {
        QJsonObject o = v.toObject();
        s_cppPatterns[o["id"].toString()] = o;
    }

    s_crossLangNote = root["cross_language_note"].toObject();
}

// Returns the why_it_matters text for a pattern at the given level (1-2 uses slot "1", 3-4 uses slot "3").
// Supports both plain "why_it_matters" and "why_it_matters_template" (template substituted by caller).
static QString getWhyText(const QJsonObject& pattern, int level, const QString& slot)
{
    QJsonObject levels = pattern["levels"].toObject();
    QString key = (level <= 2) ? "1" : "3";
    QJsonObject levelObj = levels[key].toObject();

    // Prefer non-template first, fall back to template key
    QString text = levelObj[slot].toString();
    if (text.isEmpty())
        text = levelObj[slot + "_template"].toString();
    return text;
}

// ============================================================================
// Public entry point
// ============================================================================

QList<OptimizationEntry> CodeOptimizer::analyzeFile(const QString& code,
                                                     const QString& language,
                                                     int level) const
{
    loadOptimizerData();
    QStringList lines = code.split('\n');

    if (language == "python")
        return analyzePython(lines, level);
    if (language == "c" || language == "cpp")
        return analyzeCpp(lines, language == "cpp", level);

    return {};
}

// ============================================================================
// Cross-language note
// ============================================================================

QString CodeOptimizer::crossLanguageNote(const QString& fasterLang,
                                          double speedRatio,
                                          int level)
{
    loadOptimizerData();
    QString ratio = QString::number(speedRatio, 'f', 1);
    QString fasterUpper = fasterLang.toUpper();

    QString tmpl = (level <= 2) ? s_crossLangNote["level_1_2"].toString()
                                 : s_crossLangNote["level_3_4"].toString();
    return tmpl.arg(ratio, fasterUpper);
}

// ============================================================================
// Helper: is 'name' used anywhere after line defLine (0-based)?
// ============================================================================

bool CodeOptimizer::nameUsedAfterLine(const QStringList& lines,
                                      const QString& name,
                                      int defLine) const
{
    QRegularExpression re(QString("\\b%1\\b").arg(QRegularExpression::escape(name)));
    for (int i = defLine + 1; i < lines.size(); ++i) {
        if (re.match(lines[i]).hasMatch())
            return true;
    }
    return false;
}

// ============================================================================
// Python analysis
// ============================================================================

QList<OptimizationEntry> CodeOptimizer::analyzePython(const QStringList& lines,
                                                       int level) const
{
    QList<OptimizationEntry> results;

    // ── 1. Unused imports ────────────────────────────────────────────────
    {
        QJsonObject pat = s_pythonPatterns["unused_import"].toObject();
        QRegularExpression importRe(R"(^\s*import\s+(\w+)(?:\s+as\s+(\w+))?\s*$)");
        QRegularExpression fromImportRe(R"(^\s*from\s+\S+\s+import\s+(\w+)(?:\s+as\s+(\w+))?\s*$)");

        for (int i = 0; i < lines.size(); ++i) {
            const QString& line = lines[i];
            QString used;
            auto m = importRe.match(line);
            if (m.hasMatch()) {
                used = m.captured(2).isEmpty() ? m.captured(1) : m.captured(2);
            } else {
                auto m2 = fromImportRe.match(line);
                if (!m2.hasMatch()) continue;
                used = m2.captured(2).isEmpty() ? m2.captured(1) : m2.captured(2);
            }

            if (!nameUsedAfterLine(lines, used, i)) {
                OptimizationEntry e;
                e.title        = pat["title"].toString();
                e.description  = pat["description_template"].toString().arg(used);
                e.whyItMatters = getWhyText(pat, level, "why_it_matters");
                e.originalCode  = line.trimmed();
                e.suggestedCode = pat["fix_template"].toString().arg(used, line.trimmed());
                e.lineNumber    = i + 1;
                results.append(e);
            }
        }
    }

    // ── 2. String concatenation in loop ─────────────────────────────────
    {
        QJsonObject pat = s_pythonPatterns["string_concat_in_loop"].toObject();
        QRegularExpression loopRe(R"(^\s*(for|while)\s)");
        QRegularExpression strConcatRe(R"((\w+)\s*\+=\s*[\"'])");
        bool inLoop = false;
        int loopIndent = 0;

        for (int i = 0; i < lines.size(); ++i) {
            const QString& line = lines[i];
            if (loopRe.match(line).hasMatch()) {
                inLoop = true;
                int indent = 0;
                while (indent < line.size() && line[indent] == ' ') ++indent;
                loopIndent = indent;
            } else if (inLoop) {
                int indent = 0;
                while (indent < line.size() && line[indent] == ' ') ++indent;
                if (!line.trimmed().isEmpty() && indent <= loopIndent)
                    inLoop = false;
            }

            if (inLoop) {
                auto m = strConcatRe.match(line);
                if (m.hasMatch()) {
                    QString varName = m.captured(1);
                    OptimizationEntry e;
                    e.title        = pat["title"].toString();
                    e.description  = pat["description_template"].toString().arg(varName);
                    e.whyItMatters = getWhyText(pat, level, "why_it_matters").arg(varName);
                    e.originalCode  = line.trimmed();
                    e.suggestedCode = pat["fix_template"].toString().arg(varName);
                    e.lineNumber    = i + 1;
                    results.append(e);
                    inLoop = false;
                }
            }
        }
    }

    // ── 3. List used for 'in' membership check ───────────────────────────
    {
        QJsonObject pat = s_pythonPatterns["list_membership"].toObject();
        QRegularExpression listAssignRe(R"(^\s*(\w+)\s*=\s*\[)");
        QRegularExpression memberCheckRe(R"(\bif\s+\w+\s+in\s+(\w+))");
        QSet<QString> listVars;

        for (int i = 0; i < lines.size(); ++i) {
            const QString& line = lines[i];
            auto m = listAssignRe.match(line);
            if (m.hasMatch())
                listVars.insert(m.captured(1));

            auto m2 = memberCheckRe.match(line);
            if (m2.hasMatch() && listVars.contains(m2.captured(1))) {
                QString varName = m2.captured(1);
                OptimizationEntry e;
                e.title        = pat["title"].toString();
                e.description  = pat["description_template"].toString().arg(varName);
                e.whyItMatters = getWhyText(pat, level, "why_it_matters").arg(varName);
                e.originalCode  = line.trimmed();
                e.suggestedCode = pat["fix_template"].toString().arg(varName, line.trimmed());
                e.lineNumber    = i + 1;
                results.append(e);
                listVars.remove(varName);
            }
        }
    }

    // ── 4. O(n²) nested loops over same collection ───────────────────────
    {
        QJsonObject pat = s_pythonPatterns["nested_loops_same_collection"].toObject();
        QRegularExpression forIterRe(R"(^\s*for\s+\w+\s+in\s+(\w+)\s*:)");

        for (int i = 0; i < lines.size(); ++i) {
            const QString& line = lines[i];
            auto m = forIterRe.match(line);
            if (!m.hasMatch()) continue;

            QString collection = m.captured(1);
            int outerIndent = 0;
            while (outerIndent < line.size() && line[outerIndent] == ' ') ++outerIndent;

            for (int j = i + 1; j < lines.size() && j < i + 40; ++j) {
                const QString& inner = lines[j];
                if (inner.trimmed().isEmpty()) continue;
                int innerIndent = 0;
                while (innerIndent < inner.size() && inner[innerIndent] == ' ') ++innerIndent;
                if (innerIndent <= outerIndent) break;

                auto m2 = forIterRe.match(inner);
                if (m2.hasMatch() && m2.captured(1) == collection) {
                    OptimizationEntry e;
                    e.title        = pat["title"].toString();
                    e.description  = pat["description_template"].toString().arg(collection);
                    e.whyItMatters = getWhyText(pat, level, "why_it_matters").arg(collection);
                    e.originalCode  = line.trimmed() + "\n    " + inner.trimmed();
                    e.suggestedCode = pat["fix_template"].toString().arg(collection);
                    e.lineNumber    = i + 1;
                    results.append(e);
                    break;
                }
            }
        }
    }

    // ── 5. Missing type hints on function params ─────────────────────────
    {
        QJsonObject pat = s_pythonPatterns["missing_type_hints"].toObject();
        QRegularExpression defRe(R"(^\s*def\s+(\w+)\s*\(([^)]*)\)\s*:)");

        for (int i = 0; i < lines.size(); ++i) {
            auto m = defRe.match(lines[i]);
            if (!m.hasMatch()) continue;
            QString params = m.captured(2).trimmed();
            if (params.isEmpty() || params == "self" || params == "cls") continue;
            bool missingHints = false;
            for (const QString& p : params.split(',')) {
                QString pt = p.trimmed();
                if (pt == "self" || pt == "cls" || pt.startsWith("*") || pt.isEmpty()) continue;
                if (!pt.contains(':') && !pt.contains('=')) { missingHints = true; break; }
            }
            if (missingHints) {
                OptimizationEntry e;
                e.title        = pat["title"].toString();
                e.description  = pat["description_template"].toString().arg(m.captured(1));
                e.whyItMatters = getWhyText(pat, level, "why_it_matters");
                e.originalCode  = QString("def %1(%2):").arg(m.captured(1), params);
                e.suggestedCode = pat["fix_template"].toString().arg(m.captured(1));
                e.lineNumber    = i + 1;
                results.append(e);
            }
        }
    }

    // ── 6. Global variable access inside functions ───────────────────────
    {
        QJsonObject pat = s_pythonPatterns["global_variable"].toObject();
        QRegularExpression globalRe(R"(^\s*global\s+(\w+))");

        for (int i = 0; i < lines.size(); ++i) {
            auto m = globalRe.match(lines[i]);
            if (!m.hasMatch()) continue;
            QString varName = m.captured(1);
            OptimizationEntry e;
            e.title        = pat["title"].toString();
            e.description  = pat["description_template"].toString().arg(varName);
            e.whyItMatters = getWhyText(pat, level, "why_it_matters").arg(varName);
            e.originalCode  = lines[i].trimmed();
            e.suggestedCode = pat["fix_template"].toString().arg(varName);
            e.lineNumber    = i + 1;
            results.append(e);
        }
    }

    // ── 7. Import inside a function ──────────────────────────────────────
    {
        QJsonObject pat = s_pythonPatterns["import_in_function"].toObject();
        QRegularExpression funcBodyImportRe(
            R"(^(\s{4,})import\s+|^(\s{4,})from\s+\S+\s+import\s+)");

        for (int i = 0; i < lines.size(); ++i) {
            if (!funcBodyImportRe.match(lines[i]).hasMatch()) continue;
            OptimizationEntry e;
            e.title        = pat["title"].toString();
            e.description  = pat["description"].toString();
            e.whyItMatters = getWhyText(pat, level, "why_it_matters");
            e.originalCode  = lines[i].trimmed();
            e.suggestedCode = pat["fix_template"].toString().arg(lines[i].trimmed());
            e.lineNumber    = i + 1;
            results.append(e);
        }
    }

    return results;
}

// ============================================================================
// C / C++ analysis
// ============================================================================

QList<OptimizationEntry> CodeOptimizer::analyzeCpp(const QStringList& lines,
                                                    bool isCpp,
                                                    int level) const
{
    QList<OptimizationEntry> results;

    // ── 1. Non-const variables never modified ────────────────────────────
    {
        QJsonObject pat = s_cppPatterns["non_const_variable"].toObject();
        QRegularExpression varDeclRe(
            R"(^\s*(?:int|double|float|long|char|bool|size_t)\s+(\w+)\s*=)");

        for (int i = 0; i < lines.size(); ++i) {
            auto m = varDeclRe.match(lines[i]);
            if (!m.hasMatch()) continue;
            QString varName = m.captured(1);
            QRegularExpression reassignRe(
                QString(R"(\b%1\s*(?:\+|-|\*|/|%|&|\||\^)?=(?!=))")
                    .arg(QRegularExpression::escape(varName)));
            bool reassigned = false;
            for (int j = i + 1; j < lines.size(); ++j) {
                if (reassignRe.match(lines[j]).hasMatch()) { reassigned = true; break; }
            }
            if (!reassigned) {
                OptimizationEntry e;
                e.title        = pat["title"].toString();
                e.description  = pat["description_template"].toString().arg(varName);
                e.whyItMatters = getWhyText(pat, level, "why_it_matters");
                e.originalCode  = lines[i].trimmed();
                e.suggestedCode = pat["fix_prefix"].toString() + lines[i].trimmed();
                e.lineNumber    = i + 1;
                results.append(e);
            }
        }
    }

    // ── 2. malloc/calloc for single objects ──────────────────────────────
    {
        QJsonObject pat = s_cppPatterns["malloc_single_object"].toObject();
        QRegularExpression mallocRe(R"(\bmalloc\s*\(\s*sizeof\s*\(\s*(\w+)\s*\)\s*\))");

        for (int i = 0; i < lines.size(); ++i) {
            auto m = mallocRe.match(lines[i]);
            if (!m.hasMatch()) continue;
            QString type = m.captured(1);
            OptimizationEntry e;
            e.title        = pat["title"].toString();
            e.description  = pat["description_template"].toString().arg(type);
            e.whyItMatters = getWhyText(pat, level, "why_it_matters").arg(type);
            e.originalCode  = lines[i].trimmed();
            e.suggestedCode = pat["fix_template"].toString().arg(type);
            e.lineNumber    = i + 1;
            results.append(e);
        }
    }

    // ── 3. Pass-by-value of large types (C++ only) ───────────────────────
    if (isCpp) {
        QJsonObject pat = s_cppPatterns["pass_by_value"].toObject();
        QRegularExpression paramRe(R"(^\s*\w[\w:<>]*\s+(\w+)\s*\(([^)]+)\))");
        static QRegularExpression primRe(
            R"(^(?:int|long|short|char|float|double|bool|void|size_t|uint|unsigned)\b)");

        for (int i = 0; i < lines.size(); ++i) {
            const QString& line = lines[i];
            if (!paramRe.match(line).hasMatch()) continue;
            int parenOpen  = line.indexOf('(');
            int parenClose = line.lastIndexOf(')');
            if (parenOpen < 0 || parenClose < 0) continue;
            QString params = line.mid(parenOpen + 1, parenClose - parenOpen - 1);
            for (const QString& p : params.split(',')) {
                QString pt = p.trimmed();
                if (pt.contains('&') || pt.contains('*') || pt.isEmpty()) continue;
                if (primRe.match(pt).hasMatch()) continue;
                if (pt.isEmpty() || !pt[0].isUpper()) continue;
                QString typeName = pt.split(' ')[0];
                OptimizationEntry e;
                e.title        = pat["title"].toString();
                e.description  = pat["description_template"].toString().arg(typeName);
                e.whyItMatters = getWhyText(pat, level, "why_it_matters").arg(typeName);
                e.originalCode  = line.trimmed();
                QString suggested = line;
                suggested.replace(typeName, "const " + typeName + "&");
                e.suggestedCode = suggested.trimmed();
                e.lineNumber    = i + 1;
                results.append(e);
                break;
            }
        }
    }

    // ── 4. Small functions that could be inline ──────────────────────────
    {
        QJsonObject pat = s_cppPatterns["small_function_inline"].toObject();
        QRegularExpression funcOpenRe(
            R"(^\s*\w[\w:<>*&\s]+\s+(\w+)\s*\([^)]*\)\s*\{?\s*$)");

        for (int i = 0; i < lines.size(); ++i) {
            const QString& line = lines[i];
            if (line.contains("inline") || line.contains("//")) continue;
            auto m = funcOpenRe.match(line);
            if (!m.hasMatch()) continue;

            int braceDepth = 0, bodyLines = 0;
            for (int j = i; j < lines.size() && j < i + 20; ++j) {
                braceDepth += lines[j].count('{') - lines[j].count('}');
                if (j > i) ++bodyLines;
                if (j > i && braceDepth <= 0) break;
            }
            QString funcName = m.captured(1);
            if (funcName.isEmpty() || funcName == "main" ||
                funcName == "if" || funcName == "for" || funcName == "while") continue;

            int callCount = 0;
            QRegularExpression callRe(
                QString(R"(\b%1\s*\()").arg(QRegularExpression::escape(funcName)));
            for (const QString& l : lines)
                if (callRe.match(l).hasMatch()) ++callCount;

            if (bodyLines > 0 && bodyLines <= 4 && callCount >= 2) {
                OptimizationEntry e;
                e.title       = pat["title"].toString();
                e.description = pat["description_template"].toString()
                    .arg(funcName).arg(bodyLines).arg(callCount - 1);
                e.whyItMatters = getWhyText(pat, level, "why_it_matters");
                e.originalCode  = line.trimmed();
                e.suggestedCode = pat["fix_prefix"].toString() + line.trimmed();
                e.lineNumber    = i + 1;
                results.append(e);
            }
        }
    }

    return results;
}
