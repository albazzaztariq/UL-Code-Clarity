#pragma once

#include <QObject>
#include <QString>
#include <QList>

// ── PossibleCause ─────────────────────────────────────────────────────────
struct PossibleCause {
    QString title;          // Short name, e.g. "Variable not defined"
    QString explanation;    // Human-readable explanation
    int     relevantLine;   // Line number in source (0 = unknown)
    QString suggestedFix;   // Short fix suggestion
};

// ── WhatsWrongAnalyzer ────────────────────────────────────────────────────
// Pattern-matching analyzer for common runtime errors.
// Given an error message, source code, and the line where it crashed,
// returns a ranked list of PossibleCause objects.
//
// Usage:
//   WhatsWrongAnalyzer a;
//   auto causes = a.analyzeError("NameError: name 'x' is not defined", code, 10);

class WhatsWrongAnalyzer : public QObject {
    Q_OBJECT

public:
    explicit WhatsWrongAnalyzer(QObject* parent = nullptr);

    QList<PossibleCause> analyzeError(const QString& errorMessage,
                                       const QString& code,
                                       int crashLine) const;

private:
    // Individual pattern handlers
    PossibleCause nameError(const QString& msg, int line) const;
    PossibleCause indexError(const QString& msg, const QString& code, int line) const;
    PossibleCause typeError(const QString& msg, int line) const;
    PossibleCause zeroDivisionError(int line) const;
    PossibleCause attributeError(const QString& msg, int line) const;
    PossibleCause fileNotFoundError(const QString& msg, int line) const;
    PossibleCause segfaultError(int line) const;
    PossibleCause valueError(const QString& msg, int line) const;
    PossibleCause keyError(const QString& msg, int line) const;
    PossibleCause recursionError(int line) const;
    PossibleCause importError(const QString& msg, int line) const;
    PossibleCause indentationError(int line) const;
    PossibleCause syntaxError(int line) const;
};
