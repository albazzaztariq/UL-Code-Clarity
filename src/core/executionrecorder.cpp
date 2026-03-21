#include "core/executionrecorder.h"

ExecutionRecorder::ExecutionRecorder(QObject* parent)
    : QObject(parent)
{
    m_buffer.resize(MAX_SNAPSHOTS);
}

void ExecutionRecorder::record(int line,
                                const QMap<QString, QVariant>& variables,
                                const QStringList& callStack)
{
    // Memory warning if variables are unusually large
    if (variables.size() > MEMORY_WARN_VARS) {
        emit memoryWarning(
            QString("Watch out: %1 variables in scope. Recording may use significant memory.")
                .arg(variables.size()));
    }

    ExecutionSnapshot snap;
    snap.line      = line;
    snap.variables = variables;
    snap.callStack = callStack;

    m_buffer[m_head] = snap;
    m_head = (m_head + 1) % MAX_SNAPSHOTS;
    if (m_count < MAX_SNAPSHOTS)
        ++m_count;

    // New recording resets the rewind cursor to "at latest"
    m_position = 0;
}

bool ExecutionRecorder::stepBack(ExecutionSnapshot& snap)
{
    // Can step back up to m_count - 1 steps from the latest
    if (m_position >= m_count - 1)
        return false;

    ++m_position;

    // Index of the snapshot at m_position steps before the last written
    // head - 1 is the most recent; head - 1 - m_position is m_position steps older
    int idx = ((m_head - 1 - m_position) % MAX_SNAPSHOTS + MAX_SNAPSHOTS) % MAX_SNAPSHOTS;
    snap = m_buffer[idx];
    return true;
}

bool ExecutionRecorder::stepForward(ExecutionSnapshot& snap)
{
    if (m_position <= 0)
        return false;

    --m_position;

    int idx = ((m_head - 1 - m_position) % MAX_SNAPSHOTS + MAX_SNAPSHOTS) % MAX_SNAPSHOTS;
    snap = m_buffer[idx];
    return true;
}

void ExecutionRecorder::reset()
{
    m_head     = 0;
    m_count    = 0;
    m_position = 0;
    // Clear contents to free memory
    for (auto& s : m_buffer) {
        s.line = 0;
        s.variables.clear();
        s.callStack.clear();
    }
}
