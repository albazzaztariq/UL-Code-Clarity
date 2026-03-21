#pragma once

#include <QString>
#include <QList>

// ============================================================================
// SecurityFinding — one detected security issue
// ============================================================================
struct SecurityFinding {
    enum Severity { LOW, MEDIUM, HIGH, CRITICAL };

    Severity severity      = MEDIUM;
    QString  title;          // Short name e.g. "SQL Injection Risk"
    QString  description;    // Plain-English explanation (level-aware)
    QString  whyItMatters;   // What an attacker could DO with this (level-aware)
    int      lineNumber    = -1; // 1-based, -1 = unknown
    QString  fixSuggestion;  // What to change
    QString  matchedText;    // The offending snippet
};

// ============================================================================
// StaticAnalyzer — SAST engine
//
// analyzeCode(code, language, filePath, level) returns all SecurityFindings.
// Runs cppcheck silently for C/C++ if installed; falls back to regex scanning.
// Regex scanning is always used as a baseline for all languages.
//
// language: "python", "c", "cpp", "javascript", "ul"
// filePath: path to the file on disk (needed for cppcheck); empty = skip tool
// level: 1-4 (affects description verbosity, read from QSettings if -1)
//
// Attribution:
//   attributionName() — "cppcheck" or "" (if regex only)
//   attributionUrl()  — URL to tool homepage
// ============================================================================
class StaticAnalyzer {
public:
    StaticAnalyzer() = default;

    QList<SecurityFinding> analyzeCode(const QString& code,
                                       const QString& language,
                                       const QString& filePath = QString(),
                                       int level = -1) const;

    // Set after analyzeCode() — which backend(s) ran
    QString attributionName() const { return m_attributionName; }
    QString attributionUrl()  const { return m_attributionUrl; }

private:
    // Per-language regex scanners
    QList<SecurityFinding> scanPython(const QStringList& lines, int level) const;
    QList<SecurityFinding> scanC(const QStringList& lines, int level) const;
    QList<SecurityFinding> scanJavaScript(const QStringList& lines, int level) const;
    QList<SecurityFinding> scanAllLanguages(const QStringList& lines, int level) const;

    // External tool backends (silent — user never sees tool names in UI)
    QList<SecurityFinding> runCppcheck(const QString& filePath, int level) const;

    // Helper: resolve assist level from QSettings if level == -1
    static int resolveLevel(int level);

    // Mutable so const analyzeCode() can set them
    mutable QString m_attributionName;
    mutable QString m_attributionUrl;
};
