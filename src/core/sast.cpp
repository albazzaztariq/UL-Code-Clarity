#include "core/sast.h"
#include "core/jsonloader.h"

#include <QRegularExpression>
#include <QSettings>
#include <QProcess>
#include <QXmlStreamReader>
#include <QJsonArray>
#include <QJsonObject>

// ── Pattern storage ───────────────────────────────────────────────────────────

struct SastLevelText {
    QString description;
    QString whyItMatters;
    QString fix;
};

struct SastPattern {
    QString id;
    QRegularExpression regex;
    SecurityFinding::Severity severity = SecurityFinding::MEDIUM;
    QString title;
    QString vulnType;
    bool skipComments  = false;
    bool extraCondition = false; // pattern-specific extra check needed
    QHash<int, SastLevelText> levels; // 1..4
};

// Shared loaded pattern lists
static QVector<SastPattern> s_allPatterns;
static QVector<SastPattern> s_pythonPatterns;
static QVector<SastPattern> s_cPatterns;
static QVector<SastPattern> s_jsPatterns;
static bool s_patternsLoaded = false;

static SecurityFinding::Severity parseSeverity(const QString& s)
{
    if (s == "CRITICAL") return SecurityFinding::CRITICAL;
    if (s == "HIGH")     return SecurityFinding::HIGH;
    if (s == "LOW")      return SecurityFinding::LOW;
    return SecurityFinding::MEDIUM;
}

static SastPattern parsePattern(const QJsonObject& po)
{
    SastPattern p;
    p.id      = po["id"].toString();
    p.title   = po["title"].toString();
    p.vulnType = po["vuln_type"].toString();
    p.severity = parseSeverity(po["severity"].toString());
    p.skipComments = po["skip_comments"].toBool(false);

    QRegularExpression::PatternOptions opts;
    if (po["flags"].toString().contains("case_insensitive"))
        opts |= QRegularExpression::CaseInsensitiveOption;
    p.regex = QRegularExpression(po["regex"].toString(), opts);

    QJsonObject levels = po["levels"].toObject();
    for (const QString& key : levels.keys()) {
        int lvl = key.toInt();
        QJsonObject lo = levels[key].toObject();
        SastLevelText lt;
        lt.description  = lo["description"].toString();
        lt.whyItMatters = lo["why_it_matters"].toString();
        lt.fix          = lo["fix"].toString();
        p.levels[lvl]   = lt;
    }
    return p;
}

static void loadPatterns()
{
    if (s_patternsLoaded) return;
    s_patternsLoaded = true;

    QJsonObject root = JsonLoader::loadObject("patterns_sast.json");

    auto loadList = [&](const QString& key, QVector<SastPattern>& out) {
        for (const QJsonValue& v : root[key].toArray())
            out.append(parsePattern(v.toObject()));
    };

    loadList("all_languages", s_allPatterns);
    loadList("python", s_pythonPatterns);
    loadList("c", s_cPatterns);
    loadList("javascript", s_jsPatterns);
}

// ── Level resolver ───────────────────────────────────────────────────────────
int StaticAnalyzer::resolveLevel(int level)
{
    if (level >= 1 && level <= 4) return level;
    QSettings s("CodeClarity", "CodeClarity");
    return s.value("assistLevel", 1).toInt();
}

// ── Helper: fill finding text from loaded pattern ────────────────────────────
static void fillFindingText(SecurityFinding& f, const SastPattern& p, int level)
{
    // Find the closest available level (prefer exact, then fall back to highest <= level, then lowest)
    const QHash<int, SastLevelText>& lvls = p.levels;
    int best = -1;
    for (int k : lvls.keys()) {
        if (k <= level && k > best) best = k;
    }
    if (best < 0 && !lvls.isEmpty()) best = lvls.keys().first();
    if (best < 0) return;

    const SastLevelText& lt = lvls[best];
    f.description  = lt.description;
    f.whyItMatters = lt.whyItMatters;
    f.fixSuggestion = lt.fix;
}

// ── Generic line scanner ─────────────────────────────────────────────────────
static QList<SecurityFinding> scanWithPatterns(const QStringList& lines,
                                                const QVector<SastPattern>& patterns,
                                                int level)
{
    QList<SecurityFinding> findings;
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        QString trimmed = line.trimmed();

        for (const SastPattern& p : patterns) {
            if (p.skipComments) {
                if (trimmed.startsWith('#') || trimmed.startsWith("//"))
                    continue;
            }

            auto m = p.regex.match(line);
            if (!m.hasMatch()) continue;

            SecurityFinding f;
            f.severity    = p.severity;
            f.title       = p.title;
            f.vulnType    = p.vulnType;
            f.lineNumber  = i + 1;
            f.matchedText = m.captured(0);
            fillFindingText(f, p, level);
            findings.append(f);
        }
    }
    return findings;
}

// ── Public entry point ───────────────────────────────────────────────────────
QList<SecurityFinding> StaticAnalyzer::analyzeCode(const QString& code,
                                                    const QString& language,
                                                    const QString& filePath,
                                                    int level) const
{
    loadPatterns();
    int lvl = resolveLevel(level);
    QStringList lines = code.split('\n');
    QList<SecurityFinding> findings;
    m_attributionName.clear();
    m_attributionUrl.clear();

    findings += scanAllLanguages(lines, lvl);

    if (language == "python") {
        findings += scanPython(lines, lvl);
    } else if (language == "c" || language == "cpp" || language == "ul") {
        findings += scanC(lines, lvl);
        if (!filePath.isEmpty()) {
            QList<SecurityFinding> cppcheckFindings = runCppcheck(filePath, lvl);
            if (!cppcheckFindings.isEmpty()) {
                QSet<int> regexLines;
                for (const auto& f : findings)
                    if (f.lineNumber > 0) regexLines.insert(f.lineNumber);
                for (const auto& cf : cppcheckFindings)
                    if (cf.lineNumber < 1 || !regexLines.contains(cf.lineNumber))
                        findings.append(cf);
                m_attributionName = "cppcheck";
                m_attributionUrl  = "https://cppcheck.sourceforge.io/";
            }
        }
    } else if (language == "javascript") {
        findings += scanJavaScript(lines, lvl);
    }

    return findings;
}

// ── Per-language scan methods (now data-driven) ──────────────────────────────

QList<SecurityFinding> StaticAnalyzer::scanAllLanguages(const QStringList& lines, int level) const
{
    return scanWithPatterns(lines, s_allPatterns, level);
}

QList<SecurityFinding> StaticAnalyzer::scanPython(const QStringList& lines, int level) const
{
    // Most patterns are generic; yaml.load needs an extra condition check
    QList<SecurityFinding> findings;
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        if (line.trimmed().startsWith('#')) continue;

        for (const SastPattern& p : s_pythonPatterns) {
            auto m = p.regex.match(line);
            if (!m.hasMatch()) continue;

            // yaml_load: exclude if Loader= is already specified
            if (p.id == "yaml_load" && line.contains("Loader=")) continue;

            // printf_user: exclude if format string literal is already provided
            if (p.id == "printf_user" && (line.contains("printf(\"%") || line.contains("printf('")))
                continue;

            SecurityFinding f;
            f.severity    = p.severity;
            f.title       = p.title;
            f.vulnType    = p.vulnType;
            f.lineNumber  = i + 1;
            f.matchedText = m.captured(0);
            fillFindingText(f, p, level);
            findings.append(f);
        }
    }
    return findings;
}

QList<SecurityFinding> StaticAnalyzer::scanC(const QStringList& lines, int level) const
{
    QList<SecurityFinding> findings;

    // Find the malloc_no_check pattern separately (needs lookahead)
    const SastPattern* mallocNoCheckPattern = nullptr;
    for (const SastPattern& p : s_cPatterns)
        if (p.id == "malloc_no_check") { mallocNoCheckPattern = &p; break; }

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        QString trimmed = line.trimmed();
        if (trimmed.startsWith("//") || trimmed.startsWith("/*") || trimmed.startsWith('*'))
            continue;

        for (const SastPattern& p : s_cPatterns) {
            if (p.id == "malloc_no_check") {
                // Special: check if next few lines have a null check
                auto m = p.regex.match(line);
                if (!m.hasMatch()) continue;

                bool hasNullCheck = false;
                for (int j = i + 1; j < qMin(i + 4, lines.size()); ++j) {
                    QString next = lines[j].trimmed();
                    if (next.isEmpty()) continue;
                    if (next.contains("== NULL") || next.contains("!= NULL") ||
                        next.contains("== nullptr") || next.contains("!= nullptr") ||
                        next.contains("if (") || next.contains("if(") || next.contains("assert("))
                        hasNullCheck = true;
                    break;
                }
                if (hasNullCheck) continue;

                SecurityFinding f;
                f.severity    = p.severity;
                f.title       = p.title;
                f.vulnType    = p.vulnType;
                f.lineNumber  = i + 1;
                f.matchedText = m.captured(0);
                fillFindingText(f, p, level);
                findings.append(f);
                continue;
            }

            // printf_user: exclude if format string literal provided
            if (p.id == "printf_user" && (line.contains("printf(\"%") || line.contains("printf('")))
                continue;

            auto m = p.regex.match(line);
            if (!m.hasMatch()) continue;

            SecurityFinding f;
            f.severity    = p.severity;
            f.title       = p.title;
            f.vulnType    = p.vulnType;
            f.lineNumber  = i + 1;
            f.matchedText = m.captured(0);
            fillFindingText(f, p, level);
            findings.append(f);
        }
    }
    return findings;
}

QList<SecurityFinding> StaticAnalyzer::scanJavaScript(const QStringList& lines, int level) const
{
    return scanWithPatterns(lines, s_jsPatterns, level);
}

// ── cppcheck backend ─────────────────────────────────────────────────────────
QList<SecurityFinding> StaticAnalyzer::runCppcheck(const QString& filePath, int level) const
{
    QList<SecurityFinding> findings;

    QProcess probe;
    probe.start("cppcheck", {"--version"});
    if (!probe.waitForFinished(3000) || probe.exitCode() != 0)
        return findings;

    QProcess proc;
    QStringList args = {
        "--enable=all", "--inconclusive", "--xml", "--xml-version=2",
        "--suppress=missingIncludeSystem", "--suppress=unusedFunction",
        filePath
    };
    proc.start("cppcheck", args);
    if (!proc.waitForFinished(30000))
        return findings;

    QByteArray xmlData = proc.readAllStandardError();
    if (xmlData.isEmpty())
        return findings;

    // Load cppcheck severity lists from JSON
    QJsonObject root = JsonLoader::loadObject("patterns_sast.json");
    QJsonObject cppcheckCfg = root["cppcheck_severities"].toObject();

    QStringList skipIds, criticalIds, highIds;
    for (const QJsonValue& v : cppcheckCfg["skip_ids"].toArray())   skipIds   << v.toString();
    for (const QJsonValue& v : cppcheckCfg["critical_ids"].toArray()) criticalIds << v.toString();
    for (const QJsonValue& v : cppcheckCfg["high_ids"].toArray())   highIds   << v.toString();

    // Level-based text for common cppcheck finding categories
    struct CppcheckText { QString title; SastLevelText levels[3]; }; // 0=L1, 1=L2, 2=L3+

    QXmlStreamReader xml(xmlData);
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != QLatin1String("error"))
            continue;

        QString id       = xml.attributes().value("id").toString();
        QString severity = xml.attributes().value("severity").toString();
        QString msg      = xml.attributes().value("msg").toString();
        int lineNum      = -1;

        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QLatin1String("location"))
                lineNum = xml.attributes().value("line").toInt();
            if (xml.isEndElement() && xml.name() == QLatin1String("error"))
                break;
        }

        if (skipIds.contains(id)) continue;

        SecurityFinding::Severity sev = SecurityFinding::MEDIUM;
        if (severity == "error")                                  sev = SecurityFinding::HIGH;
        else if (severity == "style" || severity == "performance"
                 || severity == "portability")                    sev = SecurityFinding::LOW;
        if (criticalIds.contains(id))                            sev = SecurityFinding::CRITICAL;
        else if (highIds.contains(id))                           sev = SecurityFinding::HIGH;

        SecurityFinding f;
        f.severity    = sev;
        f.lineNumber  = lineNum;
        f.matchedText = id;

        // Map cppcheck id to human text
        if (id.contains("bufferAccess") || id.contains("outOfBounds") || id.contains("arrayIndex")) {
            f.title = "Buffer Out-of-Bounds Access";
            if (level == 1) {
                f.description   = "The program accesses memory outside an array's boundary.";
                f.whyItMatters  = "Attackers craft input that causes the out-of-bounds write to land on the return address.";
                f.fixSuggestion = "Always check index < array size before accessing.";
            } else if (level == 2) {
                f.description   = "Array/buffer out-of-bounds access — undefined behavior and potential exploit.";
                f.whyItMatters  = "Out-of-bounds writes are a core memory corruption primitive enabling arbitrary code execution.";
                f.fixSuggestion = "Add bounds checks; std::vector::at() for automatic range checking.";
            } else {
                f.description   = "Buffer/array OOB (CWE-125/787).";
                f.whyItMatters  = "Memory corruption → RCE via heap/stack smash.";
                f.fixSuggestion = "Bounds check; std::vector::at().";
            }
        } else if (id.contains("nullPointer")) {
            f.title = "Null Pointer Dereference";
            if (level == 1) {
                f.description   = "The program might use a NULL pointer — reading from address zero crashes the program.";
                f.whyItMatters  = "Crashes enable denial of service.";
                f.fixSuggestion = "Check the pointer is not NULL before using it.";
            } else {
                f.description   = level == 2 ? "Potential null pointer dereference (CWE-476)." : "Null pointer (CWE-476).";
                f.whyItMatters  = "DoS / potential privilege escalation.";
                f.fixSuggestion = "Null check before dereference.";
            }
        } else if (id.contains("memleak") || id.contains("resourceLeak")) {
            f.title = "Memory / Resource Leak";
            if (level == 1) {
                f.description   = "Memory is allocated but never released — the program uses more and more memory over time.";
                f.whyItMatters  = "An attacker can trigger the leaking code repeatedly to exhaust server memory.";
                f.fixSuggestion = "Every malloc() must have a matching free().";
            } else {
                f.description   = "Memory/resource leak (CWE-401).";
                f.whyItMatters  = "DoS via resource exhaustion.";
                f.fixSuggestion = "RAII (unique_ptr) or explicit free().";
            }
        } else if (id == "doubleFree") {
            f.title    = "Double Free";
            f.severity = SecurityFinding::CRITICAL;
            f.description   = level == 1 ? "The same memory block is freed twice — this corrupts the memory manager."
                                         : "free() called twice on same pointer — heap corruption (CWE-415).";
            f.whyItMatters  = level == 1 ? "Skilled attackers exploit double-free for full code execution."
                                         : "Heap corruption → ACE.";
            f.fixSuggestion = "Set the pointer to NULL immediately after free().";
        } else if (id == "useAfterFree") {
            f.title    = "Use After Free";
            f.severity = SecurityFinding::CRITICAL;
            f.vulnType = "use_after_free";
            f.description   = level == 1 ? "The program uses memory after it has been freed."
                                         : "Pointer used after free() — UAF (CWE-416).";
            f.whyItMatters  = level == 1 ? "Attackers can control what is placed in the freed block."
                                         : "ACE via dangling pointer.";
            f.fixSuggestion = "Set the pointer to NULL after free() and never use it again.";
        } else if (id.contains("uninit")) {
            f.title = "Uninitialized Variable Used";
            f.description   = level == 1 ? "A variable is used before being given a value."
                                         : "Uninitialised variable (CWE-457).";
            f.whyItMatters  = level == 1 ? "The random value might be a previous user's password on the stack."
                                         : "Info leak via stale stack data.";
            f.fixSuggestion = "Always initialize variables when you declare them.";
        } else if (id.contains("formatString")) {
            f.title    = "Format String Vulnerability";
            f.severity = SecurityFinding::HIGH;
            f.description   = level == 1 ? "A variable is used directly as the format in printf()."
                                         : "Format string vuln (CWE-134).";
            f.whyItMatters  = level == 1 ? "Using %x reads stack values; %n writes to arbitrary addresses."
                                         : "Memory disclosure / write-what-where → RCE.";
            f.fixSuggestion = "printf(\"%s\", variable) — always provide an explicit format string.";
        } else {
            f.title = QString("Code Defect (%1)").arg(id);
            f.description   = level == 1 ? "A potential problem was detected: " + msg : msg;
            f.whyItMatters  = "Memory/control-flow defects may be exploitable.";
            f.fixSuggestion = "Review the flagged code.";
        }

        findings.append(f);
    }

    return findings;
}
