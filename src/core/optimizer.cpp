#include "core/optimizer.h"

#include <QRegularExpression>
#include <QSet>

// ============================================================================
// Public entry point
// ============================================================================
QList<OptimizationEntry> CodeOptimizer::analyzeFile(const QString& code,
                                                     const QString& language,
                                                     int /*level*/) const
{
    QStringList lines = code.split('\n');

    if (language == "python")
        return analyzePython(lines);
    if (language == "c" || language == "cpp")
        return analyzeCpp(lines, language == "cpp");

    return {};
}

// ============================================================================
// Cross-language note
// ============================================================================
QString CodeOptimizer::crossLanguageNote(const QString& fasterLang, double speedRatio)
{
    QString ratio = QString::number(speedRatio, 'f', 1);
    return QString(
        "Python ran %1x slower than the %2 version.\n\n"
        "This is expected — Python interprets bytecode at runtime while %2 compiles "
        "directly to native machine code. For production use of this workload, consider "
        "C, Rust, or UL (which compiles to C). Python is best suited for rapid prototyping."
    ).arg(ratio, fasterLang.toUpper());
}

// ============================================================================
// Helper: is 'name' used anywhere after line defLine (0-based)?
// ============================================================================
bool CodeOptimizer::nameUsedAfterLine(const QStringList& lines,
                                      const QString& name,
                                      int defLine) const
{
    // Whole-word match
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
QList<OptimizationEntry> CodeOptimizer::analyzePython(const QStringList& lines) const
{
    QList<OptimizationEntry> results;

    // ── 1. Unused imports ────────────────────────────────────────────────
    // Patterns: "import foo", "import foo as bar", "from x import foo"
    QRegularExpression importRe(R"(^\s*import\s+(\w+)(?:\s+as\s+(\w+))?\s*$)");
    QRegularExpression fromImportRe(R"(^\s*from\s+\S+\s+import\s+(\w+)(?:\s+as\s+(\w+))?\s*$)");

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];

        auto m = importRe.match(line);
        if (m.hasMatch()) {
            // The name actually used in code is the alias (if any) else the module
            QString used = m.captured(2).isEmpty() ? m.captured(1) : m.captured(2);
            if (!nameUsedAfterLine(lines, used, i)) {
                OptimizationEntry e;
                e.title        = "Unused import";
                e.description  = QString("'%1' is imported but never used. "
                    "Unused imports slow down startup and clutter the namespace.")
                    .arg(used);
                e.originalCode = line.trimmed();
                e.suggestedCode = QString("# Remove: %1").arg(line.trimmed());
                e.lineNumber   = i + 1;
                results.append(e);
            }
            continue;
        }

        auto m2 = fromImportRe.match(line);
        if (m2.hasMatch()) {
            QString used = m2.captured(2).isEmpty() ? m2.captured(1) : m2.captured(2);
            if (!nameUsedAfterLine(lines, used, i)) {
                OptimizationEntry e;
                e.title        = "Unused import";
                e.description  = QString("'%1' is imported but never used. "
                    "Unused imports slow down startup and clutter the namespace.")
                    .arg(used);
                e.originalCode = line.trimmed();
                e.suggestedCode = QString("# Remove: %1").arg(line.trimmed());
                e.lineNumber   = i + 1;
                results.append(e);
            }
        }
    }

    // ── 2. String concatenation in loop ─────────────────────────────────
    // Detect: result += "..." or result = result + "..." inside a for/while block
    QRegularExpression loopRe(R"(^\s*(for|while)\s)");
    QRegularExpression strConcatRe(R"((\w+)\s*\+=\s*[\"'])");
    bool inLoop = false;
    int  loopIndent = 0;

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        if (loopRe.match(line).hasMatch()) {
            inLoop = true;
            // Measure indent of the loop header
            int indent = 0;
            while (indent < line.size() && line[indent] == ' ') ++indent;
            loopIndent = indent;
        } else if (inLoop) {
            // Check we're still inside the loop body (indent > loopIndent)
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
                e.title        = "String concatenation in loop";
                e.description  = QString(
                    "'%1 +=' inside a loop creates a new string object each iteration, "
                    "making this O(n²). Use a list and join at the end instead.")
                    .arg(varName);
                e.originalCode  = line.trimmed();
                e.suggestedCode = QString("parts = []\n"
                    "    parts.append(...)  # inside loop\n"
                    "%1 = ''.join(parts)  # after loop").arg(varName);
                e.lineNumber   = i + 1;
                results.append(e);
                inLoop = false; // report once per loop
            }
        }
    }

    // ── 3. list used for 'in' membership check in a loop ────────────────
    // Detect: "if x in someVar" where someVar was assigned as a list literal
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
            e.title        = "Use set instead of list for membership test";
            e.description  = QString(
                "'if x in %1' where %1 is a list is O(n) per check. "
                "If %1 doesn't change, convert it to a set for O(1) lookups.")
                .arg(varName);
            e.originalCode  = line.trimmed();
            e.suggestedCode = QString("%1 = set(%1)  # convert once before the loop\n"
                                      "%2").arg(varName, line.trimmed());
            e.lineNumber   = i + 1;
            results.append(e);
            listVars.remove(varName); // report once per variable
        }
    }

    // ── 4. O(n²) nested loops over same collection ───────────────────────
    // Detect: two nested for loops iterating over the same variable
    QRegularExpression forIterRe(R"(^\s*for\s+\w+\s+in\s+(\w+)\s*:)");
    QStringList outerCollections;

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        auto m = forIterRe.match(line);
        if (!m.hasMatch()) continue;

        QString collection = m.captured(1);
        int outerIndent = 0;
        while (outerIndent < line.size() && line[outerIndent] == ' ') ++outerIndent;

        // Look ahead for a nested for loop over the same collection
        for (int j = i + 1; j < lines.size() && j < i + 40; ++j) {
            const QString& inner = lines[j];
            if (inner.trimmed().isEmpty()) continue;
            int innerIndent = 0;
            while (innerIndent < inner.size() && inner[innerIndent] == ' ') ++innerIndent;
            if (innerIndent <= outerIndent) break; // left the outer loop body

            auto m2 = forIterRe.match(inner);
            if (m2.hasMatch() && m2.captured(1) == collection) {
                OptimizationEntry e;
                e.title        = "O(n\xC2\xB2) nested loop over same collection";
                e.description  = QString(
                    "Two nested loops both iterating over '%1' gives O(n\xC2\xB2) complexity. "
                    "Consider sorting + two-pointer, a hash map, or itertools.combinations.")
                    .arg(collection);
                e.originalCode  = line.trimmed() + "\n    " + inner.trimmed();
                e.suggestedCode = "# Consider: sort + two-pointer, dict lookup, or\n"
                                  "# itertools.combinations(collection, 2)";
                e.lineNumber   = i + 1;
                results.append(e);
                break;
            }
        }
    }

    // ── 5. Missing type hints on function params ─────────────────────────
    QRegularExpression defRe(R"(^\s*def\s+(\w+)\s*\(([^)]*)\)\s*:)");
    for (int i = 0; i < lines.size(); ++i) {
        auto m = defRe.match(lines[i]);
        if (!m.hasMatch()) continue;
        QString params = m.captured(2).trimmed();
        if (params.isEmpty() || params == "self" || params == "cls") continue;
        // If any param lacks a colon annotation and isn't self/cls
        QStringList paramList = params.split(',');
        bool missingHints = false;
        for (const QString& p : paramList) {
            QString pt = p.trimmed();
            if (pt == "self" || pt == "cls" || pt.startsWith("*") || pt.isEmpty()) continue;
            if (!pt.contains(':') && !pt.contains('=')) {
                missingHints = true;
                break;
            }
        }
        if (missingHints) {
            OptimizationEntry e;
            e.title        = "Missing type hints";
            e.description  = QString(
                "Function '%1' has parameters without type annotations. "
                "Type hints help static analysis tools catch bugs early and "
                "make intent clearer.")
                .arg(m.captured(1));
            e.originalCode  = QString("def %1(%2):").arg(m.captured(1), params);
            e.suggestedCode = QString("def %1(%2) -> ReturnType:  # add :Type to each param")
                .arg(m.captured(1), params);
            e.lineNumber   = i + 1;
            results.append(e);
        }
    }

    // ── 6. Global variable access inside functions ───────────────────────
    QRegularExpression globalRe(R"(^\s*global\s+(\w+))");
    for (int i = 0; i < lines.size(); ++i) {
        auto m = globalRe.match(lines[i]);
        if (!m.hasMatch()) continue;
        OptimizationEntry e;
        e.title        = "Global variable access in function";
        e.description  = QString(
            "'global %1' causes the interpreter to look up the variable in global scope "
            "on every access, which is slower than a local. Pass it as a parameter instead.")
            .arg(m.captured(1));
        e.originalCode  = lines[i].trimmed();
        e.suggestedCode = QString("# Pass '%1' as a function parameter instead of using global")
            .arg(m.captured(1));
        e.lineNumber   = i + 1;
        results.append(e);
    }

    // ── 7. Import inside a function ──────────────────────────────────────
    QRegularExpression funcBodyImportRe(R"(^(\s{4,})import\s+|^(\s{4,})from\s+\S+\s+import\s+)");
    for (int i = 0; i < lines.size(); ++i) {
        if (funcBodyImportRe.match(lines[i]).hasMatch()) {
            OptimizationEntry e;
            e.title        = "Import inside function";
            e.description  = "Importing inside a function re-runs the import machinery on "
                              "every call. Move imports to the top of the file.";
            e.originalCode  = lines[i].trimmed();
            e.suggestedCode = QString("# Move to top of file: %1").arg(lines[i].trimmed());
            e.lineNumber   = i + 1;
            results.append(e);
        }
    }

    return results;
}

// ============================================================================
// C / C++ analysis
// ============================================================================
QList<OptimizationEntry> CodeOptimizer::analyzeCpp(const QStringList& lines, bool isCpp) const
{
    QList<OptimizationEntry> results;

    // ── 1. Non-const variables never modified ────────────────────────────
    // Simple heuristic: "int x = ..." or "double x = ..." not followed by "x ="
    QRegularExpression varDeclRe(R"(^\s*(?:int|double|float|long|char|bool|size_t)\s+(\w+)\s*=)");
    for (int i = 0; i < lines.size(); ++i) {
        auto m = varDeclRe.match(lines[i]);
        if (!m.hasMatch()) continue;
        QString varName = m.captured(1);
        // Check if it's ever assigned to again
        QRegularExpression reassignRe(QString(R"(\b%1\s*(?:\+|-|\*|/|%|&|\||\^)?=(?!=))")
            .arg(QRegularExpression::escape(varName)));
        bool reassigned = false;
        for (int j = i + 1; j < lines.size(); ++j) {
            if (reassignRe.match(lines[j]).hasMatch()) { reassigned = true; break; }
        }
        if (!reassigned) {
            OptimizationEntry e;
            e.title        = "Variable could be const";
            e.description  = QString(
                "'%1' is assigned once and never modified. Marking it 'const' lets the "
                "compiler optimise it and makes intent clear.")
                .arg(varName);
            e.originalCode  = lines[i].trimmed();
            e.suggestedCode = QString("const %1").arg(lines[i].trimmed());
            e.lineNumber   = i + 1;
            results.append(e);
        }
    }

    // ── 2. malloc/calloc for small fixed sizes ───────────────────────────
    QRegularExpression mallocRe(R"(\bmalloc\s*\(\s*sizeof\s*\(\s*(\w+)\s*\)\s*\))");
    for (int i = 0; i < lines.size(); ++i) {
        auto m = mallocRe.match(lines[i]);
        if (!m.hasMatch()) continue;
        QString type = m.captured(1);
        OptimizationEntry e;
        e.title        = "Heap allocation for single object";
        e.description  = QString(
            "malloc(sizeof(%1)) allocates a single object on the heap. "
            "If the lifetime is limited to this scope, use a stack variable instead: "
            "it's faster (no allocation overhead) and automatically freed.")
            .arg(type);
        e.originalCode  = lines[i].trimmed();
        e.suggestedCode = QString("%1 obj;  // stack allocation — no malloc needed").arg(type);
        e.lineNumber   = i + 1;
        results.append(e);
    }

    // ── 3. Pass-by-value of struct/class (C++ only) ──────────────────────
    if (isCpp) {
        // Detect: function params like "MyStruct s" or "std::string s" (no & or *)
        QRegularExpression paramRe(
            R"(^\s*\w[\w:<>]*\s+(\w+)\s*\(([^)]+)\))");
        QRegularExpression byValueRe(
            R"((?:struct\s+)?(\w+)\s+(\w+)(?:\s*,|\s*\)))");

        for (int i = 0; i < lines.size(); ++i) {
            const QString& line = lines[i];
            if (!paramRe.match(line).hasMatch()) continue;
            // Look for type names in the param list that aren't primitive and lack & or *
            // Simple check: if a word-pair "TypeName varName" exists without & * const
            int parenOpen  = line.indexOf('(');
            int parenClose = line.lastIndexOf(')');
            if (parenOpen < 0 || parenClose < 0) continue;
            QString params = line.mid(parenOpen + 1, parenClose - parenOpen - 1);
            // Skip if it contains & or * for all params
            QStringList paramList = params.split(',');
            for (const QString& p : paramList) {
                QString pt = p.trimmed();
                // Must look like "SomeType varName" — not primitive, no & or *
                static QRegularExpression primRe(
                    R"(^(?:int|long|short|char|float|double|bool|void|size_t|uint|unsigned)\b)");
                if (pt.contains('&') || pt.contains('*') || pt.isEmpty()) continue;
                if (primRe.match(pt).hasMatch()) continue;
                // If it starts with an uppercase letter it's likely a user type
                if (!pt[0].isUpper()) continue;
                OptimizationEntry e;
                e.title        = "Pass large type by value";
                e.description  = QString(
                    "Passing '%1' by value copies the entire object. "
                    "Use 'const %1&' to avoid the copy overhead.").arg(pt.split(' ')[0]);
                e.originalCode  = line.trimmed();
                // Replace first occurrence of the type without &
                QString suggested = line;
                suggested.replace(pt.split(' ')[0], "const " + pt.split(' ')[0] + "&");
                e.suggestedCode = suggested.trimmed();
                e.lineNumber   = i + 1;
                results.append(e);
                break; // one report per function signature
            }
        }
    }

    // ── 4. Small functions that could be inline ──────────────────────────
    // Detect function definitions with body <= 4 lines, no inline keyword
    QRegularExpression funcOpenRe(R"(^\s*\w[\w:<>*&\s]+\s+(\w+)\s*\([^)]*\)\s*\{?\s*$)");
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        if (line.contains("inline") || line.contains("//")) continue;
        auto m = funcOpenRe.match(line);
        if (!m.hasMatch()) continue;

        // Count body lines until closing brace
        int braceDepth = 0;
        int bodyLines  = 0;
        int j = i;
        for (; j < lines.size() && j < i + 20; ++j) {
            braceDepth += lines[j].count('{') - lines[j].count('}');
            if (j > i) ++bodyLines;
            if (j > i && braceDepth <= 0) break;
        }
        // Count call sites
        QString funcName = m.captured(1);
        if (funcName.isEmpty() || funcName == "main" || funcName == "if" ||
            funcName == "for"  || funcName == "while") continue;

        int callCount = 0;
        QRegularExpression callRe(QString(R"(\b%1\s*\()").arg(
            QRegularExpression::escape(funcName)));
        for (const QString& l : lines) {
            if (callRe.match(l).hasMatch()) ++callCount;
        }

        if (bodyLines > 0 && bodyLines <= 4 && callCount >= 2) {
            OptimizationEntry e;
            e.title        = "Small function could be inline";
            e.description  = QString(
                "'%1' has a small body (%2 lines) and is called %3 times. "
                "Marking it 'inline' hints the compiler to expand it at call sites, "
                "eliminating function call overhead.")
                .arg(funcName).arg(bodyLines).arg(callCount - 1); // -1: def line itself
            e.originalCode  = line.trimmed();
            e.suggestedCode = "inline " + line.trimmed();
            e.lineNumber   = i + 1;
            results.append(e);
        }
    }

    return results;
}
