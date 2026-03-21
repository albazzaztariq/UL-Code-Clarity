#include "core/dast.h"

#include <QProcess>
#include <QSettings>
#include <QByteArray>
#include <QString>
#include <QRegularExpression>

// ── Level resolver ───────────────────────────────────────────────────────────
int DynamicTester::resolveLevel()
{
    QSettings s("CodeClarity", "CodeClarity");
    return s.value("assistLevel", 1).toInt();
}

// ── Test cases ───────────────────────────────────────────────────────────────
QList<DynamicTester::TestCase> DynamicTester::buildTestCases() const
{
    QList<TestCase> cases;

    cases.append(TestCase{"Empty input",            ""});
    cases.append(TestCase{"Very long string",       QString(10000, 'A')});
    cases.append(TestCase{"Special characters",     "!@#$%^&*(){}[]|;'\"\\<>?/"});
    cases.append(TestCase{"Negative number",        "-1"});
    cases.append(TestCase{"Zero",                   "0"});
    cases.append(TestCase{"MAX_INT",                "2147483647"});
    cases.append(TestCase{"Null bytes",             QString(QByteArray("\x00\x00\x00", 3))});
    cases.append(TestCase{"Format string attack",   "%s%s%s%n%x%x%x"});
    cases.append(TestCase{"Path traversal",         "../../etc/passwd"});
    cases.append(TestCase{"SQL injection attempt",  "' OR '1'='1"});
    cases.append(TestCase{"Shell metacharacters",   "; cat /etc/passwd"});
    cases.append(TestCase{"Very large number",      "99999999999999999999"});
    cases.append(TestCase{"Unicode overlong",       QString::fromUtf8("\xC0\xAF\xC0\xAF")});
    cases.append(TestCase{"Newline injection",      "test\ninjected"});

    return cases;
}

// ── Public entry point ───────────────────────────────────────────────────────
QList<SecurityFinding> DynamicTester::testProgram(const QString& exePath,
                                                   const QString& language) const
{
    Q_UNUSED(language);
    QList<SecurityFinding> findings;

    const auto tests = buildTestCases();
    for (const auto& tc : tests) {
        runTest(exePath, tc, findings);
    }

    return findings;
}

// ── Single test runner ───────────────────────────────────────────────────────
void DynamicTester::runTest(const QString& exePath,
                             const TestCase& tc,
                             QList<SecurityFinding>& out) const
{
    int level = resolveLevel();

    QProcess proc;
    proc.setProgram(exePath);
    proc.start();

    if (!proc.waitForStarted(3000)) {
        SecurityFinding f;
        f.severity    = SecurityFinding::HIGH;
        f.title       = "Program Failed to Start";
        f.lineNumber  = -1;
        switch (level) {
        case 1:
            f.description = "The program could not be started for testing. "
                            "Make sure it compiled correctly and is executable.";
            break;
        default:
            f.description = "QProcess failed to start the executable.";
            break;
        }
        f.fixSuggestion = "Verify the binary exists and the build succeeded.";
        out.append(f);
        return;
    }

    // Send crafted input
    QByteArray inputBytes = tc.input.toUtf8();
    if (!inputBytes.isEmpty()) {
        proc.write(inputBytes);
        proc.write("\n");
    }
    proc.closeWriteChannel();

    bool finished = proc.waitForFinished(5000); // 5-second timeout

    if (!finished) {
        proc.kill();
        proc.waitForFinished(1000);

        SecurityFinding f;
        f.severity    = SecurityFinding::HIGH;
        f.title       = "Program Hang / Infinite Loop";
        f.lineNumber  = -1;
        f.matchedText = QString("Input: %1").arg(tc.label);
        switch (level) {
        case 1:
            f.description = QString(
                "When given '%1' as input, the program hung and never stopped. "
                "Attackers can use this to make your server unresponsive (denial of service).").arg(tc.label);
            f.fixSuggestion = "Add input validation and timeouts to prevent infinite loops on unexpected input.";
            break;
        case 2:
            f.description = QString("Program timed out (>5s) on input: %1. Possible infinite loop or deadlock.").arg(tc.label);
            f.fixSuggestion = "Add input bounds checking; review loops that process user input.";
            break;
        default:
            f.description = QString("Hang on input '%1' — DoS vector.").arg(tc.label);
            f.fixSuggestion = "Input validation; timeout guards.";
            break;
        }
        out.append(f);
        return;
    }

    int exitCode = proc.exitCode();
    QByteArray stderrOutput = proc.readAllStandardError();
    QByteArray stdoutOutput = proc.readAllStandardOutput();

    // Crash detection
    if (proc.exitStatus() == QProcess::CrashExit || exitCode != 0) {
        SecurityFinding f;
        f.severity    = SecurityFinding::HIGH;
        f.title       = "Program Crash";
        f.lineNumber  = -1;
        f.matchedText = QString("Input: %1 | Exit code: %2").arg(tc.label).arg(exitCode);
        switch (level) {
        case 1:
            f.description = QString(
                "The program crashed when it received '%1' as input (exit code %2). "
                "Crashes can sometimes be turned into attacks that run the attacker's code.").arg(tc.label).arg(exitCode);
            f.fixSuggestion = "Add input validation to handle unexpected values gracefully instead of crashing.";
            break;
        case 2:
            f.description = QString("Non-zero exit (%1) on input '%2'. May indicate buffer overflow, segfault, or unhandled exception.").arg(exitCode).arg(tc.label);
            f.fixSuggestion = "Run under a debugger or valgrind with the same input to identify the crash cause.";
            break;
        case 3:
            f.description = QString("Crash (exit %1) on input '%2' — possible memory corruption or unhandled edge case.").arg(exitCode).arg(tc.label);
            f.fixSuggestion = "Investigate with GDB/AddressSanitizer.";
            break;
        default:
            f.description = QString("Exit %1 on '%2'.").arg(exitCode).arg(tc.label);
            f.fixSuggestion = "Debug with ASan/GDB.";
            break;
        }
        out.append(f);
    }

    // Stderr output — error messages / stack traces
    if (!stderrOutput.isEmpty()) {
        QString errStr = QString::fromUtf8(stderrOutput).trimmed();
        if (!errStr.isEmpty()) {
            SecurityFinding f;
            f.severity    = SecurityFinding::MEDIUM;
            f.title       = "Error Output on Stderr";
            f.lineNumber  = -1;
            f.matchedText = QString("Input: %1 | Stderr: %2").arg(tc.label).arg(errStr.left(120));
            switch (level) {
            case 1:
                f.description = QString(
                    "The program printed error messages when given '%1' as input. "
                    "Error messages can reveal internal details that help attackers plan an attack.").arg(tc.label);
                f.fixSuggestion = "Log errors internally without showing details to users. Return a generic error message instead.";
                break;
            case 2:
                f.description = QString("Stderr output on input '%1' — may expose stack traces, paths, or internal state.").arg(tc.label);
                f.fixSuggestion = "Suppress or sanitize error output to users; log internally only.";
                break;
            default:
                f.description = QString("Stderr on '%1': information disclosure risk.").arg(tc.label);
                f.fixSuggestion = "Suppress verbose errors in production.";
                break;
            }
            out.append(f);
        }
    }

    // Format string leak detection: if we sent %s%s and got memory addresses / garbage back
    if (tc.label == "Format string attack" && !stdoutOutput.isEmpty()) {
        QString outStr = QString::fromUtf8(stdoutOutput);
        // Heuristic: output contains hex patterns (0x...) which weren't in our input
        const QRegularExpression reHex(R"(0x[0-9a-fA-F]{4,})");
        if (reHex.match(outStr).hasMatch()) {
            SecurityFinding f;
            f.severity    = SecurityFinding::CRITICAL;
            f.title       = "Format String Vulnerability Confirmed";
            f.lineNumber  = -1;
            f.matchedText = QString("Output contained memory addresses: %1").arg(outStr.left(80));
            switch (level) {
            case 1:
                f.description = "The program echoed memory addresses when given %s%s as input. "
                                "An attacker can use this to read secret values from memory or craft a code-execution exploit.";
                f.fixSuggestion = "Find every printf(var) and change it to printf(\"%s\", var).";
                break;
            case 2:
                f.description = "Format string vulnerability confirmed — program leaked memory addresses on %s%s input.";
                f.fixSuggestion = "Audit all printf/fprintf calls; always use an explicit format string.";
                break;
            default:
                f.description = "Format string memory leak confirmed (CWE-134).";
                f.fixSuggestion = "Fix all printf(var) to printf(\"%s\", var).";
                break;
            }
            out.append(f);
        }
    }

    // Path traversal response detection
    if (tc.label == "Path traversal" && !stdoutOutput.isEmpty()) {
        QString outStr = QString::fromUtf8(stdoutOutput);
        if (outStr.contains("root:") || outStr.contains("/bin/") || outStr.contains("daemon:")) {
            SecurityFinding f;
            f.severity    = SecurityFinding::CRITICAL;
            f.title       = "Path Traversal Vulnerability Confirmed";
            f.lineNumber  = -1;
            switch (level) {
            case 1:
                f.description = "The program read and showed contents of /etc/passwd when given ../../etc/passwd as input. "
                                "An attacker could read any file on the server.";
                f.fixSuggestion = "Validate file paths: resolve to canonical form and check they stay within the allowed directory.";
                break;
            case 2:
                f.description = "Path traversal confirmed — program read /etc/passwd from ../../etc/passwd input.";
                f.fixSuggestion = "Use realpath() / Path.resolve() and whitelist the allowed base directory.";
                break;
            default:
                f.description = "Path traversal (CWE-22) — /etc/passwd readable.";
                f.fixSuggestion = "Canonicalize + whitelist base dir.";
                break;
            }
            out.append(f);
        }
    }
}
