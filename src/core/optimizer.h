#pragma once

#include <QString>
#include <QList>

// ============================================================================
// OptimizationEntry — one detected issue with original/suggested code snippets
// ============================================================================
struct OptimizationEntry {
    QString title;           // Short name, e.g. "Unused import"
    QString description;     // One-sentence summary of what's wrong
    QString whyItMatters;    // Teaching paragraph: WHY this makes code slower and HOW the fix helps
    QString originalCode;    // The offending snippet (1-3 lines)
    QString suggestedCode;   // The improved version
    int     lineNumber = -1; // 1-based line where the issue was detected (-1 = unknown)
};

// ============================================================================
// CodeOptimizer — regex/line-scan based static analysis
//
// analyzeFile(code, language, level) returns a list of OptimizationEntry.
// This is NOT a full parser — it uses pattern matching to catch common
// beginner mistakes in Python and C/C++.
//
// Supported language strings: "python", "c", "cpp"
// level: 1-4 (1-2 = beginner language in whyItMatters, 3-4 = technical detail)
// ============================================================================
class CodeOptimizer {
public:
    CodeOptimizer() = default;

    QList<OptimizationEntry> analyzeFile(const QString& code,
                                          const QString& language,
                                          int level = 1) const;

    // Generates a cross-language comparison note when Python is compared to C/C++/Rust
    // speedRatio: how many times slower Python was (e.g. 47.0 means 47x slower)
    // level: 1-2 = beginner-friendly prose, 3-4 = technical
    static QString crossLanguageNote(const QString& fasterLang, double speedRatio,
                                     int level = 1);

private:
    QList<OptimizationEntry> analyzePython(const QStringList& lines, int level) const;
    QList<OptimizationEntry> analyzeCpp(const QStringList& lines, bool isCpp, int level) const;

    // Helper: check if a name appears outside its definition line
    bool nameUsedAfterLine(const QStringList& lines, const QString& name, int defLine) const;
};
