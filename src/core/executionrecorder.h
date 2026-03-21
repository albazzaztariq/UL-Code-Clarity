#pragma once

#include <QObject>
#include <QMap>
#include <QVariant>
#include <QStringList>
#include <QVector>

// ── ExecutionSnapshot ─────────────────────────────────────────────────────
struct ExecutionSnapshot {
    int                     line      = 0;
    QMap<QString, QVariant> variables;
    QStringList             callStack;
};

// ── ExecutionRecorder ─────────────────────────────────────────────────────
// Stores the last MAX_SNAPSHOTS execution snapshots in a circular buffer.
// Supports reverse debugging: stepBack() and stepForward() through history.
//
// Usage:
//   recorder->record(line, vars, stack);   // call on every debugger line event
//   ExecutionSnapshot snap;
//   if (recorder->stepBack(snap)) { /* use snap */ }

class ExecutionRecorder : public QObject {
    Q_OBJECT

public:
    static constexpr int MAX_SNAPSHOTS = 1000;
    static constexpr int MEMORY_WARN_VARS = 500;

    explicit ExecutionRecorder(QObject* parent = nullptr);

    // Record a new snapshot at the current execution point
    void record(int line,
                const QMap<QString, QVariant>& variables,
                const QStringList& callStack);

    // Step backwards through history; fills snap and returns true if available
    bool stepBack(ExecutionSnapshot& snap);

    // Step forwards through history; fills snap and returns true if available
    bool stepForward(ExecutionSnapshot& snap);

    // Total recorded snapshots
    int count() const { return m_count; }

    // Current position in history (0 = latest)
    int position() const { return m_position; }

    // Clear all history
    void reset();

signals:
    void memoryWarning(const QString& message);

private:
    QVector<ExecutionSnapshot> m_buffer;   // circular buffer of MAX_SNAPSHOTS
    int m_head     = 0;   // index of next-write slot
    int m_count    = 0;   // total valid entries
    int m_position = 0;   // cursor: 0 = at head (latest), positive = going back
};
