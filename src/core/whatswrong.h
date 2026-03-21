#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QVector>
#include <QJsonObject>

// ── PossibleCause ─────────────────────────────────────────────────────────
struct PossibleCause {
    QString title;          // Short name, e.g. "Variable not defined"
    QString explanation;    // Human-readable explanation
    int     relevantLine;   // Line number in source (0 = unknown)
    QString suggestedFix;   // Short fix suggestion
};

// ── WhatsWrongAnalyzer ────────────────────────────────────────────────────
// Pattern-matching analyzer for common runtime errors.
// Error patterns are loaded from data/errors.json at construction time.
// Given an error message, source code, and the line where it crashed,
// returns a list of PossibleCause objects.
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
    QVector<QJsonObject> m_errorTypes;  // loaded from data/errors.json
};
