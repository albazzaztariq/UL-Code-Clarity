#pragma once

#include "core/sast.h"
#include <QString>
#include <QList>

// ============================================================================
// DynamicTester — DAST engine
//
// testProgram(exePath, language) runs the compiled program with crafted attack
// inputs via QProcess and returns findings for any observed bad behaviour.
//
// Detected:
//   - Crash (non-zero exit code)
//   - Hang (timeout > 5 s)
//   - Stderr output (error messages / stack traces)
//   - Suspicious stdout (format string leak, path traversal echo)
// ============================================================================
class DynamicTester {
public:
    DynamicTester() = default;

    QList<SecurityFinding> testProgram(const QString& exePath,
                                       const QString& language) const;

private:
    struct TestCase {
        QString label;       // human-readable name of the test
        QString input;       // stdin to send
    };

    QList<TestCase> buildTestCases() const;

    // Run a single test case; appends any findings to out
    void runTest(const QString& exePath,
                 const TestCase& tc,
                 QList<SecurityFinding>& out) const;

    static int resolveLevel();
};
