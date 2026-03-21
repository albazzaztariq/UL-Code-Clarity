#include "core/sast.h"

#include <QRegularExpression>
#include <QSettings>

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
                                                    int level) const
{
    int lvl = resolveLevel(level);
    QStringList lines = code.split('\n');
    QList<SecurityFinding> findings;

    findings += scanAllLanguages(lines, lvl);

    if (language == "python") {
        findings += scanPython(lines, lvl);
    } else if (language == "c" || language == "cpp" || language == "ul") {
        findings += scanC(lines, lvl);
    } else if (language == "javascript") {
        findings += scanJavaScript(lines, lvl);
    }

    return findings;
}

// ── All-language checks ──────────────────────────────────────────────────────
QList<SecurityFinding> StaticAnalyzer::scanAllLanguages(const QStringList& lines, int level) const
{
    QList<SecurityFinding> findings;

    // Hardcoded password / secret / key patterns
    static const QRegularExpression rePassword(
        R"((password|passwd|pwd|secret|api_?key|token|auth)\s*[=:]\s*[\"'][^\"']{4,}[\"'])",
        QRegularExpression::CaseInsensitiveOption);

    // TODO / FIXME / HACK comments that could indicate incomplete security work
    static const QRegularExpression reTodo(
        R"(\b(TODO|FIXME|HACK|XXX)\b.*?(auth|secur|password|cred|sql|inject|sanitize|escape|validat))",
        QRegularExpression::CaseInsensitiveOption);

    // Hardcoded IP / local path patterns (platform-specific paths)
    static const QRegularExpression rePath(
        R"([\"'](C:\\|/etc/|/home/|/root/|/var/|D:\\)[^\"']+[\"'])");

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];

        // Skip comment-only lines for credential check (crude: starts with # or //)
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
                    f.description = "This line has a password or secret key written directly in the code. "
                                    "Anyone who reads the code (or its git history) can see it.";
                    f.fixSuggestion = "Move the secret to an environment variable and read it with os.environ or a .env file.";
                    break;
                case 2:
                    f.description = "Hardcoded credentials in source. The value is visible in source control history even after removal.";
                    f.fixSuggestion = "Use environment variables or a secrets manager. Never commit credentials.";
                    break;
                case 3:
                    f.description = "Credential embedded as string literal — exposed in source and version history.";
                    f.fixSuggestion = "Externalise to env var / secrets store; add pattern to .gitignore-style secret scanner.";
                    break;
                default:
                    f.description = "Hardcoded credential (CWE-798).";
                    f.fixSuggestion = "Externalise secret; rotate immediately if already committed.";
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
                f.description = "There is a note in the code saying something security-related hasn't been finished yet.";
                f.fixSuggestion = "Complete or remove the unfinished security work before shipping.";
                break;
            case 2:
                f.description = "TODO/FIXME/HACK comment near security-sensitive code. Incomplete security is a vulnerability.";
                f.fixSuggestion = "Resolve or create a tracked issue; don't ship with known security gaps.";
                break;
            default:
                f.description = "Security-related TODO comment — potential incomplete mitigation.";
                f.fixSuggestion = "Resolve before release; track in issue tracker.";
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
                f.description = "A file path specific to one computer is written into the code. "
                                "This won't work on anyone else's machine and could expose directory structure.";
                f.fixSuggestion = "Use a configuration file or environment variable for paths.";
                break;
            default:
                f.description = "Platform-specific hardcoded path — non-portable and potentially information-leaking.";
                f.fixSuggestion = "Use config/env var; prefer relative paths or path-joining APIs.";
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
        QString trimmed = line.trimmed();
        if (trimmed.startsWith('#')) continue;

        struct Check {
            const QRegularExpression* re;
            SecurityFinding::Severity sev;
            const char* title;
            const char* desc1;
            const char* desc2;
            const char* desc3;
            const char* desc4;
            const char* fix;
        };

        // eval
        if (reEval.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::CRITICAL;
            f.title      = "Code Injection via eval()";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description = "eval() runs whatever text you give it as Python code. "
                                "If an attacker controls that text, they can run any command on your computer.";
                f.fixSuggestion = "Never pass user input to eval(). Use safer alternatives like ast.literal_eval() for data.";
                break;
            case 2:
                f.description = "eval() executes arbitrary Python. If user-controlled input reaches it, this is a remote code execution vulnerability.";
                f.fixSuggestion = "Replace with ast.literal_eval() for data parsing, or redesign to avoid dynamic evaluation.";
                break;
            case 3:
                f.description = "eval() with potentially untrusted input — RCE risk (CWE-95).";
                f.fixSuggestion = "Use ast.literal_eval() or a safe expression parser. Sandbox if eval is required.";
                break;
            default:
                f.description = "eval() — RCE (CWE-95). Avoid or sandbox.";
                f.fixSuggestion = "ast.literal_eval() / restrict to trusted input.";
                break;
            }
            findings.append(f);
        }

        // exec
        if (reExec.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::CRITICAL;
            f.title      = "Code Injection via exec()";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description = "exec() runs code from a string, just like eval(). "
                                "An attacker who controls the input can take over your program.";
                f.fixSuggestion = "Avoid exec() with any user-supplied data. Redesign to use explicit function calls instead.";
                break;
            case 2:
                f.description = "exec() executes arbitrary Python code — potential RCE if input is not fully trusted.";
                f.fixSuggestion = "Avoid exec(); use explicit logic instead of dynamic code generation.";
                break;
            default:
                f.description = "exec() — RCE risk (CWE-95).";
                f.fixSuggestion = "Avoid dynamic code execution.";
                break;
            }
            findings.append(f);
        }

        // subprocess shell=True
        if (reShellTrue.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::HIGH;
            f.title      = "Command Injection (subprocess shell=True)";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description = "shell=True passes the command through your computer's shell, "
                                "which means special characters like ; & | can run extra commands. "
                                "An attacker can use this to run anything on your system.";
                f.fixSuggestion = "Use shell=False (the default) and pass the command as a list: subprocess.run(['cmd', 'arg1']).";
                break;
            case 2:
                f.description = "subprocess with shell=True enables shell injection if any part of the command is user-controlled.";
                f.fixSuggestion = "Use shell=False and pass arguments as a list. Use shlex.quote() if shell is required.";
                break;
            default:
                f.description = "subprocess shell=True — command injection if input is untrusted (CWE-78).";
                f.fixSuggestion = "shell=False + list args; shlex.quote() if shell required.";
                break;
            }
            findings.append(f);
        }

        // pickle.loads
        if (rePickle.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::CRITICAL;
            f.title      = "Arbitrary Code Execution via pickle";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description = "pickle can run code hidden inside data. "
                                "Loading pickled data from an untrusted source lets an attacker run anything on your computer.";
                f.fixSuggestion = "Only unpickle data you created yourself. Use JSON or another safe format for untrusted data.";
                break;
            case 2:
                f.description = "pickle.loads() on untrusted data enables arbitrary code execution — the pickle format is not safe.";
                f.fixSuggestion = "Use JSON, MessagePack, or another safe serialisation format for untrusted input.";
                break;
            default:
                f.description = "pickle.loads() — ACE from malicious payload (CWE-502).";
                f.fixSuggestion = "Replace with json.loads() or equivalent safe deserializer.";
                break;
            }
            findings.append(f);
        }

        // os.system
        if (reOsSystem.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::HIGH;
            f.title      = "Command Injection via os.system()";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description = "os.system() runs a shell command as a string. "
                                "If any part of the string comes from a user, an attacker can inject extra commands.";
                f.fixSuggestion = "Use subprocess.run(['cmd', 'arg']) with a list instead. It's safer and more flexible.";
                break;
            case 2:
                f.description = "os.system() passes command to shell — injection risk if any argument is user-controlled.";
                f.fixSuggestion = "Prefer subprocess.run() with list args and shell=False.";
                break;
            default:
                f.description = "os.system() — shell injection (CWE-78). Prefer subprocess.";
                f.fixSuggestion = "subprocess.run(['cmd', ...], shell=False).";
                break;
            }
            findings.append(f);
        }

        // SQL concatenation
        if (reSqlConcat.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::CRITICAL;
            f.title      = "SQL Injection Risk";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description = "This line lets someone put dangerous commands in your database query. "
                                "An attacker could steal all your data, delete records, or even take over the database server.";
                f.fixSuggestion = "Use parameterized queries: cursor.execute('SELECT ... WHERE id = ?', (user_id,)) — never build SQL by joining strings.";
                break;
            case 2:
                f.description = "Unsanitized user input passed to SQL query via string concatenation. Use parameterized queries.";
                f.fixSuggestion = "Replace string building with parameterized queries (? placeholders) or an ORM.";
                break;
            case 3:
                f.description = "SQL injection via string concatenation in query builder (CWE-89).";
                f.fixSuggestion = "Parameterize all user-supplied values; never interpolate into SQL strings.";
                break;
            default:
                f.description = "SQLi: string-interpolated query (CWE-89).";
                f.fixSuggestion = "Parameterize; use ORM.";
                break;
            }
            findings.append(f);
        }

        // yaml.load without Loader
        if (reYamlLoad.match(line).hasMatch() && !line.contains("Loader=")) {
            SecurityFinding f;
            f.severity   = SecurityFinding::HIGH;
            f.title      = "Unsafe YAML Deserialization";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description = "yaml.load() without a Loader can execute Python code hidden in a YAML file. "
                                "An attacker who controls the YAML can run anything on your computer.";
                f.fixSuggestion = "Use yaml.safe_load() instead of yaml.load(), or pass Loader=yaml.SafeLoader.";
                break;
            case 2:
                f.description = "yaml.load() without a safe Loader can deserialize arbitrary Python objects — RCE risk.";
                f.fixSuggestion = "Replace with yaml.safe_load() or yaml.load(data, Loader=yaml.SafeLoader).";
                break;
            default:
                f.description = "yaml.load() no Loader — ACE via crafted YAML (CWE-502).";
                f.fixSuggestion = "yaml.safe_load().";
                break;
            }
            findings.append(f);
        }

        // assert for security
        if (reAssertSec.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::HIGH;
            f.title      = "Assert Used for Security Check";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description = "assert statements are removed when Python runs in optimized mode (python -O). "
                                "If you use assert to check permissions or logins, those checks disappear in production.";
                f.fixSuggestion = "Replace assert with a proper if check that raises an exception or returns an error.";
                break;
            case 2:
                f.description = "assert is stripped in optimized builds (-O flag). Security checks using assert can be bypassed.";
                f.fixSuggestion = "Use explicit if + raise or abort() instead of assert for security-critical checks.";
                break;
            default:
                f.description = "assert for auth check — disabled under -O (CWE-617).";
                f.fixSuggestion = "Use if/raise instead.";
                break;
            }
            findings.append(f);
        }

        // verify=False (SSL bypass)
        if (reRequestsNoVerify.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::HIGH;
            f.title      = "SSL Certificate Verification Disabled";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description = "verify=False turns off the check that confirms you are talking to the real server, "
                                "not an impersonator. An attacker can intercept all your traffic (man-in-the-middle attack).";
                f.fixSuggestion = "Remove verify=False. If the cert is self-signed, provide the CA bundle with verify='/path/to/ca.crt'.";
                break;
            case 2:
                f.description = "SSL verification disabled — enables MITM attacks. Never use verify=False in production.";
                f.fixSuggestion = "Fix the certificate or provide verify='/path/to/ca.pem'. Never ship with verify=False.";
                break;
            default:
                f.description = "verify=False — MITM vulnerability (CWE-295).";
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
    static const QRegularExpression rePrintfUser(
        R"(\bprintf\s*\(\s*[a-zA-Z_][a-zA-Z0-9_]*)");  // printf(var — no format string
    static const QRegularExpression reMallocNoCheck(
        R"(\bmallocResult\s*=\s*malloc|=\s*malloc\s*\([^;]+;(?!\s*(if|assert)))");
    static const QRegularExpression reMallocNoCheckSimple(
        R"(\w+\s*=\s*malloc\s*\([^)]+\)\s*;)");
    static const QRegularExpression reMallocSize(
        R"(\bmalloc\s*\(\s*\w+\s*\*\s*sizeof)");

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        QString trimmed = line.trimmed();
        if (trimmed.startsWith("//") || trimmed.startsWith("/*") || trimmed.startsWith('*'))
            continue;

        // strcpy
        if (reStrcpy.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::HIGH;
            f.title      = "Buffer Overflow via strcpy()";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description = "strcpy() copies text without checking if it fits in the destination buffer. "
                                "Too much text will overflow into other memory, which attackers can exploit to run their own code.";
                f.fixSuggestion = "Use strncpy(dest, src, sizeof(dest) - 1) and always null-terminate, or use strlcpy() if available.";
                break;
            case 2:
                f.description = "strcpy() has no bounds checking — classic buffer overflow (CWE-120). Destination can be overflowed.";
                f.fixSuggestion = "Replace with strncpy() + explicit null termination, or snprintf(dest, size, \"%s\", src).";
                break;
            default:
                f.description = "strcpy() — buffer overflow (CWE-120).";
                f.fixSuggestion = "strncpy/strlcpy with explicit size.";
                break;
            }
            findings.append(f);
        }

        // strcat
        if (reStrcat.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::HIGH;
            f.title      = "Buffer Overflow via strcat()";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description = "strcat() adds text to a buffer without checking if there is enough space. "
                                "If the buffer is too small, memory after it gets overwritten.";
                f.fixSuggestion = "Use strncat(dest, src, remaining_space) or snprintf() to build strings safely.";
                break;
            case 2:
                f.description = "strcat() has no bounds checking — buffer overflow risk (CWE-120).";
                f.fixSuggestion = "Use strncat(dest, src, sizeof(dest) - strlen(dest) - 1) or snprintf().";
                break;
            default:
                f.description = "strcat() — buffer overflow (CWE-120).";
                f.fixSuggestion = "strncat/snprintf with size.";
                break;
            }
            findings.append(f);
        }

        // gets
        if (reGets.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::CRITICAL;
            f.title      = "Critical Buffer Overflow via gets()";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description = "gets() reads from the keyboard with no limit — it will overflow any buffer given enough input. "
                                "This is so dangerous it was removed from the C standard entirely.";
                f.fixSuggestion = "Replace with fgets(buf, sizeof(buf), stdin) which limits how much is read.";
                break;
            case 2:
                f.description = "gets() is unbounded — guaranteed buffer overflow for sufficiently long input. Removed in C11.";
                f.fixSuggestion = "Replace with fgets(buf, sizeof(buf), stdin).";
                break;
            default:
                f.description = "gets() — unbounded overflow (CWE-242). Removed from C11.";
                f.fixSuggestion = "fgets(buf, sizeof(buf), stdin).";
                break;
            }
            findings.append(f);
        }

        // sprintf
        if (reSprintf.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::MEDIUM;
            f.title      = "Potential Buffer Overflow via sprintf()";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description = "sprintf() writes formatted text to a buffer without checking if it fits. "
                                "A large value can overflow the buffer.";
                f.fixSuggestion = "Use snprintf(buf, sizeof(buf), format, ...) which limits output to the buffer size.";
                break;
            case 2:
                f.description = "sprintf() lacks bounds checking — buffer overflow if formatted output exceeds buffer size.";
                f.fixSuggestion = "Replace with snprintf(buf, sizeof(buf), fmt, ...).";
                break;
            default:
                f.description = "sprintf() — overflow if output > buffer (CWE-120).";
                f.fixSuggestion = "snprintf() with explicit size.";
                break;
            }
            findings.append(f);
        }

        // printf(variable) — format string vulnerability
        auto m = rePrintfUser.match(line);
        if (m.hasMatch() && !line.contains("printf(\"%") && !line.contains("printf('")) {
            SecurityFinding f;
            f.severity   = SecurityFinding::HIGH;
            f.title      = "Format String Vulnerability";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description = "printf() is being called with a variable as the format string instead of a fixed \"%s\" format. "
                                "If an attacker controls the variable, they can read memory or crash the program using % codes.";
                f.fixSuggestion = "Always use a format string: printf(\"%s\", user_string) — never printf(user_string).";
                break;
            case 2:
                f.description = "printf(user_input) — format string attack if the argument contains % specifiers (CWE-134).";
                f.fixSuggestion = "Use printf(\"%s\", var) with an explicit format string.";
                break;
            default:
                f.description = "Format string vulnerability (CWE-134).";
                f.fixSuggestion = "printf(\"%s\", var).";
                break;
            }
            findings.append(f);
        }

        // malloc without null check — look for malloc assignment followed by immediate use
        if (reMallocSize.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::MEDIUM;
            f.title      = "Integer Overflow in malloc Size";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description = "malloc(n * sizeof(...)) can overflow if n is very large, wrapping around to a small number. "
                                "The buffer will be too small but the code will think it is big enough.";
                f.fixSuggestion = "Check that n <= SIZE_MAX / sizeof(type) before multiplying, or use calloc(n, sizeof(type)) which handles this.";
                break;
            case 2:
                f.description = "malloc(n * sizeof) — if n is attacker-controlled, integer overflow produces an undersized allocation.";
                f.fixSuggestion = "Use calloc(n, sizeof(T)) or validate n <= SIZE_MAX/sizeof(T) first.";
                break;
            default:
                f.description = "malloc(n*sizeof) — integer overflow in size calc (CWE-190).";
                f.fixSuggestion = "calloc() or overflow check.";
                break;
            }
            findings.append(f);
        }

        // malloc without null check (simple: = malloc(...);)
        if (reMallocNoCheckSimple.match(line).hasMatch()) {
            // Peek next non-empty line for null check
            bool hasNullCheck = false;
            for (int j = i + 1; j < qMin(i + 4, lines.size()); ++j) {
                QString next = lines[j].trimmed();
                if (next.isEmpty()) continue;
                if (next.contains("== NULL") || next.contains("!= NULL") ||
                    next.contains("== nullptr") || next.contains("!= nullptr") ||
                    next.contains("if (") || next.contains("if(") ||
                    next.contains("assert(")) {
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
                    f.description = "malloc() can return NULL if the computer runs out of memory. "
                                    "Using a NULL pointer crashes the program or can be exploited.";
                    f.fixSuggestion = "Always check: if (ptr == NULL) { /* handle error */ } right after malloc().";
                    break;
                case 2:
                    f.description = "malloc() return value not checked for NULL — dereferencing it causes undefined behavior or crash.";
                    f.fixSuggestion = "Add: if (!ptr) { perror(\"malloc\"); exit(EXIT_FAILURE); }";
                    break;
                default:
                    f.description = "Unchecked malloc() — NULL dereference risk (CWE-476).";
                    f.fixSuggestion = "Check ptr != NULL before use.";
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
        QString trimmed = line.trimmed();
        if (trimmed.startsWith("//")) continue;

        if (reEval.match(line).hasMatch()) {
            SecurityFinding f;
            f.severity   = SecurityFinding::CRITICAL;
            f.title      = "Code Injection via eval()";
            f.lineNumber = i + 1;
            switch (level) {
            case 1:
                f.description = "eval() runs text as JavaScript code. "
                                "If an attacker controls the text, they can steal cookies, redirect users, or take over the page.";
                f.fixSuggestion = "Never use eval() with user data. Use JSON.parse() for data, or redesign to avoid dynamic code.";
                break;
            case 2:
                f.description = "eval() executes arbitrary JS — XSS / code injection if user input reaches it.";
                f.fixSuggestion = "Use JSON.parse() for data; avoid eval() entirely.";
                break;
            default:
                f.description = "eval() — code injection (CWE-95).";
                f.fixSuggestion = "JSON.parse(); avoid eval().";
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
                f.description = "Putting user text into innerHTML can run hidden scripts. "
                                "An attacker can steal session cookies or take control of the page.";
                f.fixSuggestion = "Use textContent instead of innerHTML when inserting user-provided text.";
                break;
            case 2:
                f.description = "innerHTML assignment with potentially user-controlled value — XSS risk (CWE-79).";
                f.fixSuggestion = "Use textContent for text; DOMPurify.sanitize() if HTML is required.";
                break;
            default:
                f.description = "innerHTML — XSS (CWE-79).";
                f.fixSuggestion = "textContent or DOMPurify.sanitize().";
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
                f.description = "document.write() inserts raw HTML. "
                                "If any user text goes in, attackers can inject scripts.";
                f.fixSuggestion = "Use DOM methods (createElement, appendChild) instead of document.write().";
                break;
            case 2:
                f.description = "document.write() with user-controlled content — XSS risk (CWE-79).";
                f.fixSuggestion = "Use DOM manipulation APIs (createElement/textContent) instead.";
                break;
            default:
                f.description = "document.write() — XSS (CWE-79).";
                f.fixSuggestion = "DOM APIs instead.";
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
                f.description = "Setting window.location from a variable can redirect users to a malicious site. "
                                "Attackers use this to trick users into giving up passwords.";
                f.fixSuggestion = "Validate the redirect target against a list of allowed URLs before redirecting.";
                break;
            case 2:
                f.description = "window.location set from variable — open redirect if attacker can control value (CWE-601).";
                f.fixSuggestion = "Whitelist allowed redirect targets; reject unexpected URLs.";
                break;
            default:
                f.description = "Unvalidated redirect (CWE-601).";
                f.fixSuggestion = "Whitelist allowed destinations.";
                break;
            }
            findings.append(f);
        }
    }

    return findings;
}
