#pragma once

#include "core/analysisframe.h"
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QString>
#include <QProcess>

// ============================================================================
// DataTraceFrame — "Trace This Variable" panel
//
// Shows every line in the current file where the chosen variable is assigned,
// modified, or passed as an argument. For Python files an optional dynamic
// trace (sys.settrace) can be triggered to capture live values.
//
// Layout:
//   [Back to Editor]       Trace the Data       [Run Live Trace]
//   Variable: [__________] [Trace]
//   ─────────────────────────────────────────────────────────────
//   Cards (one per transformation):
//     Line N  |  action description  |  value if known
//   ─────────────────────────────────────────────────────────────
// ============================================================================

// ── One step in a variable's lifetime ────────────────────────────────────────
struct TraceStep {
    int     lineNumber  = 0;
    QString action;          // e.g. "Assigned", "Passed to function", "Returned"
    QString value;           // "" if unknown (static) or known (dynamic)
    bool    isError = false; // true when a cast / operation would fail at this point
};

// ── Card widget for one step ──────────────────────────────────────────────────
class TraceCard : public QWidget {
    Q_OBJECT
public:
    explicit TraceCard(const TraceStep& step, QWidget* parent = nullptr);

signals:
    void lineJumpRequested(int line);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    TraceStep m_step;
};

// ── Main frame ────────────────────────────────────────────────────────────────
class DataTraceFrame : public AnalysisFrame {
    Q_OBJECT

public:
    explicit DataTraceFrame(QWidget* parent = nullptr);

    // Set code + language before showing
    void setCode(const QString& code, const QString& language, const QString& filePath);

    // Pre-fill the variable name (e.g. from editor selection)
    void setVariable(const QString& varName);

private slots:
    void onTrace();
    void onRunLiveTrace();
    void onLiveTraceOutput();
    void onLiveTraceFinished(int exitCode, QProcess::ExitStatus status);

private:
    // ── Static analysis ───────────────────────────────────────────────────────
    QList<TraceStep> staticTrace(const QString& varName,
                                 const QStringList& lines,
                                 const QString& lang) const;

    // ── UI helpers ────────────────────────────────────────────────────────────
    void populateCards(const QList<TraceStep>& steps);
    void clearCards();

    // ── Widgets ───────────────────────────────────────────────────────────────
    QLineEdit*   m_varInput     = nullptr;
    QPushButton* m_traceBtn     = nullptr;
    QPushButton* m_liveTraceBtn = nullptr;

    // ── State ─────────────────────────────────────────────────────────────────
    QString m_filePath;

    QProcess* m_liveProcess = nullptr;
    QString   m_liveOutput;
};
