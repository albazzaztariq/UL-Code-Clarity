#pragma once

#include <QObject>
#include <QProcess>
#include <QSet>
#include <QMap>
#include <QVariant>
#include <QTemporaryFile>

// ── DebugBackend ─────────────────────────────────────────────────────────
// Drives Python debugging via a sys.settrace wrapper script.
// Communicates with the wrapper via QProcess stdin/stdout using JSON.
//
// Events received from the wrapper (one JSON object per line on stdout):
//   {"event":"line","file":"..","line":N,"vars":{..},"stack":[..]}
//   {"event":"output","text":".."}
//   {"event":"crash","msg":"..","line":N}
//   {"event":"done"}

class DebugBackend : public QObject {
    Q_OBJECT

public:
    explicit DebugBackend(QObject* parent = nullptr);
    ~DebugBackend() override;

    // Begin a debug session — writes wrapper, starts QProcess
    void startSession(const QString& scriptPath, const QSet<int>& breakpoints);

    // Update breakpoints (effective on next continue/step)
    void setBreakpoints(const QSet<int>& lines);

    // Send a command to the wrapper:
    //   "continue" / "step" / "into" / "out" / "stop"
    void sendCommand(const QString& cmd);

    // Hard stop
    void stop();

signals:
    void lineChanged(const QString& file, int line);
    void variableUpdate(const QMap<QString, QVariant>& vars);
    void callStackChanged(const QStringList& frames);
    void output(const QString& text);
    void crashed(const QString& errorMsg, int crashLine);
    void finished();

private slots:
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QString buildWrapperScript(const QString& scriptPath, const QSet<int>& breakpoints) const;
    void parseLine(const QByteArray& line);

    QProcess*         m_process      = nullptr;
    QTemporaryFile*   m_wrapperFile  = nullptr;
    QSet<int>         m_breakpoints;
    QByteArray        m_buffer;
};
