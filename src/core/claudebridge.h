#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

// ClaudeBridge — manages a `claude` CLI subprocess with stream-json output.
// Parses NDJSON events and emits Qt signals for UI consumption.

class ClaudeBridge : public QObject {
    Q_OBJECT
public:
    explicit ClaudeBridge(QObject *parent = nullptr);
    ~ClaudeBridge();

    // Start a new session or resume an existing one.
    // If sessionId is empty, starts a fresh session.
    void startSession(const QString &sessionId = {},
                      const QString &workingDir = {});

    // Send a user message to the running session.
    void sendMessage(const QString &text);

    // Stop the claude process gracefully.
    void stop();

    bool isRunning() const;
    QString sessionId() const { return m_sessionId; }

    // Tool event details — emitted for each tool_use
    struct ToolUseEvent {
        QString id;           // tool_use_id
        QString name;         // "Edit", "Write", "Read", "Bash", etc.
        QJsonObject input;    // full parsed input (file_path, old_string, new_string, etc.)
    };

    struct ToolResultEvent {
        QString toolUseId;
        QString content;      // tool output text
        bool isError = false;
    };

signals:
    // Session lifecycle
    void sessionStarted(const QString &sessionId, const QString &model,
                        const QStringList &tools);
    void sessionEnded(double costUsd, int turns);

    // Thinking (collapsible in UI)
    void thinkingDelta(const QString &text);
    void thinkingComplete(const QString &fullThinking);

    // Assistant text response (stream into chat bubble)
    void textDelta(const QString &text);
    void textComplete(const QString &fullText);

    // Tool use (for editor tracking)
    void toolUseStarted(const ToolUseEvent &event);
    void toolInputDelta(const QString &toolId, const QString &partialJson);
    void toolUseReady(const ToolUseEvent &event);   // input fully parsed
    void toolResult(const ToolResultEvent &event);

    // Errors
    void errorOccurred(const QString &message);

    // Rate limit info
    void rateLimitInfo(const QString &status, qint64 resetsAt);

private slots:
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);

private:
    void parseLine(const QByteArray &line);
    void handleSystemEvent(const QJsonObject &obj);
    void handleStreamEvent(const QJsonObject &obj);
    void handleAssistantEvent(const QJsonObject &obj);
    void handleUserEvent(const QJsonObject &obj);
    void handleResultEvent(const QJsonObject &obj);

    QProcess *m_process = nullptr;
    QString m_sessionId;
    QByteArray m_readBuffer;

    // Accumulate partial tool input JSON
    struct PendingTool {
        QString id;
        QString name;
        QString accumulatedJson;
        int contentIndex = -1;
    };
    QMap<int, PendingTool> m_pendingTools;  // keyed by content block index

    // Accumulate thinking text
    QString m_currentThinking;
    // Accumulate response text
    QString m_currentText;
};
