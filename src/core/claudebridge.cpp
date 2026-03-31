#include "core/claudebridge.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDebug>

// ============================================================================
// Construction / Destruction
// ============================================================================

ClaudeBridge::ClaudeBridge(QObject *parent)
    : QObject(parent)
{
}

ClaudeBridge::~ClaudeBridge()
{
    stop();
}

// ============================================================================
// Session Management
// ============================================================================

void ClaudeBridge::startSession(const QString &sessionId, const QString &workingDir)
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qWarning() << "[ClaudeBridge] Process already running, stopping first";
        stop();
    }

    m_sessionId = sessionId;
    m_pendingTools.clear();
    m_currentThinking.clear();
    m_currentText.clear();
    m_readBuffer.clear();

    m_process = new QProcess(this);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &ClaudeBridge::onReadyRead);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ClaudeBridge::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &ClaudeBridge::onProcessError);

    if (!workingDir.isEmpty())
        m_process->setWorkingDirectory(workingDir);

    // Find claude executable — try multiple locations
    QString claudeExe;
    QStringList candidates = {
        "claude",  // on PATH
        QDir::homePath() + "/.claude/local/claude.exe",
        qEnvironmentVariable("LOCALAPPDATA") + "/Programs/claude/claude.exe",
        qEnvironmentVariable("APPDATA") + "/npm/claude.cmd",
    };
    for (const QString &c : candidates) {
        if (c.isEmpty()) continue;
        if (QFile::exists(c)) { claudeExe = c; break; }
    }
    if (claudeExe.isEmpty()) claudeExe = "claude";  // hope it's on PATH

    QStringList args;
    args << "--output-format" << "stream-json";

    if (!sessionId.isEmpty())
        args << "--resume" << sessionId;

    qDebug() << "[ClaudeBridge] Starting:" << claudeExe << args;
    m_process->start(claudeExe, args);

    if (!m_process->waitForStarted(5000)) {
        emit errorOccurred("Failed to start claude process: " + m_process->errorString());
        m_process->deleteLater();
        m_process = nullptr;
    }
}

void ClaudeBridge::sendMessage(const QString &text)
{
    if (!m_process || m_process->state() != QProcess::Running) {
        emit errorOccurred("Claude process is not running");
        return;
    }

    // Write message to stdin followed by newline (interactive mode)
    QByteArray data = text.toUtf8() + "\n";
    m_process->write(data);
}

void ClaudeBridge::stop()
{
    if (!m_process) return;

    if (m_process->state() == QProcess::Running) {
        m_process->write("/exit\n");
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
            m_process->waitForFinished(2000);
        }
    }
    m_process->deleteLater();
    m_process = nullptr;
}

bool ClaudeBridge::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}

// ============================================================================
// Process Signal Handlers
// ============================================================================

void ClaudeBridge::onReadyRead()
{
    m_readBuffer += m_process->readAllStandardOutput();

    // Process complete lines (NDJSON — one JSON object per line)
    while (true) {
        int newlineIdx = m_readBuffer.indexOf('\n');
        if (newlineIdx < 0) break;

        QByteArray line = m_readBuffer.left(newlineIdx).trimmed();
        m_readBuffer = m_readBuffer.mid(newlineIdx + 1);

        if (!line.isEmpty())
            parseLine(line);
    }
}

void ClaudeBridge::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status);
    if (exitCode != 0)
        emit errorOccurred(QString("Claude process exited with code %1").arg(exitCode));
    m_process->deleteLater();
    m_process = nullptr;
}

void ClaudeBridge::onProcessError(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    if (m_process)
        emit errorOccurred("Process error: " + m_process->errorString());
}

// ============================================================================
// NDJSON Parser
// ============================================================================

void ClaudeBridge::parseLine(const QByteArray &line)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "[ClaudeBridge] JSON parse error:" << err.errorString();
        return;
    }

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    if (type == "system")
        handleSystemEvent(obj);
    else if (type == "stream_event")
        handleStreamEvent(obj);
    else if (type == "assistant")
        handleAssistantEvent(obj);
    else if (type == "user")
        handleUserEvent(obj);
    else if (type == "result")
        handleResultEvent(obj);
    else if (type == "rate_limit_event") {
        QJsonObject info = obj["rate_limit_info"].toObject();
        emit rateLimitInfo(info["status"].toString(),
                           info["resetsAt"].toInteger());
    }
}

// ── System events ──────────────────────────────────────────────────────

void ClaudeBridge::handleSystemEvent(const QJsonObject &obj)
{
    QString subtype = obj["subtype"].toString();

    if (subtype == "init") {
        m_sessionId = obj["session_id"].toString();

        QStringList tools;
        for (const auto &t : obj["tools"].toArray())
            tools << t.toString();

        QString model = obj["model"].toString();
        emit sessionStarted(m_sessionId, model, tools);
    }
}

// ── Stream events (real-time deltas) ───────────────────────────────────

void ClaudeBridge::handleStreamEvent(const QJsonObject &obj)
{
    QJsonObject event = obj["event"].toObject();
    QString eventType = event["type"].toString();

    if (eventType == "content_block_start") {
        int index = event["index"].toInt();
        QJsonObject block = event["content_block"].toObject();
        QString blockType = block["type"].toString();

        if (blockType == "thinking") {
            m_currentThinking.clear();
        }
        else if (blockType == "text") {
            m_currentText.clear();
        }
        else if (blockType == "tool_use") {
            PendingTool pt;
            pt.id = block["id"].toString();
            pt.name = block["name"].toString();
            pt.contentIndex = index;
            m_pendingTools[index] = pt;

            ToolUseEvent tue;
            tue.id = pt.id;
            tue.name = pt.name;
            emit toolUseStarted(tue);
        }
    }
    else if (eventType == "content_block_delta") {
        int index = event["index"].toInt();
        QJsonObject delta = event["delta"].toObject();
        QString deltaType = delta["type"].toString();

        if (deltaType == "thinking_delta") {
            QString text = delta["thinking"].toString();
            m_currentThinking += text;
            emit thinkingDelta(text);
        }
        else if (deltaType == "text_delta") {
            QString text = delta["text"].toString();
            m_currentText += text;
            emit textDelta(text);
        }
        else if (deltaType == "input_json_delta") {
            QString partial = delta["partial_json"].toString();
            if (m_pendingTools.contains(index)) {
                m_pendingTools[index].accumulatedJson += partial;
                emit toolInputDelta(m_pendingTools[index].id, partial);
            }
        }
    }
    else if (eventType == "content_block_stop") {
        int index = event["index"].toInt();

        // Finalize thinking block
        if (!m_currentThinking.isEmpty()) {
            emit thinkingComplete(m_currentThinking);
            m_currentThinking.clear();
        }

        // Finalize text block
        if (!m_currentText.isEmpty()) {
            emit textComplete(m_currentText);
            m_currentText.clear();
        }

        // Finalize tool_use — parse accumulated JSON
        if (m_pendingTools.contains(index)) {
            PendingTool &pt = m_pendingTools[index];
            QJsonDocument inputDoc = QJsonDocument::fromJson(pt.accumulatedJson.toUtf8());

            ToolUseEvent tue;
            tue.id = pt.id;
            tue.name = pt.name;
            tue.input = inputDoc.object();
            emit toolUseReady(tue);

            m_pendingTools.remove(index);
        }
    }
}

// ── Accumulated assistant message ──────────────────────────────────────

void ClaudeBridge::handleAssistantEvent(const QJsonObject &obj)
{
    // The assistant event contains the fully accumulated message.
    // We already process everything via stream events, but this
    // provides a backup / verification point.
    Q_UNUSED(obj);
}

// ── User message (tool results) ────────────────────────────────────────

void ClaudeBridge::handleUserEvent(const QJsonObject &obj)
{
    QJsonObject message = obj["message"].toObject();
    QJsonArray content = message["content"].toArray();

    for (const auto &item : content) {
        QJsonObject block = item.toObject();
        if (block["type"].toString() == "tool_result") {
            ToolResultEvent tre;
            tre.toolUseId = block["tool_use_id"].toString();

            // Content can be a string or an array
            QJsonValue contentVal = block["content"];
            if (contentVal.isString()) {
                tre.content = contentVal.toString();
            } else if (contentVal.isArray()) {
                QStringList parts;
                for (const auto &c : contentVal.toArray()) {
                    QJsonObject co = c.toObject();
                    if (co["type"].toString() == "text")
                        parts << co["text"].toString();
                }
                tre.content = parts.join("\n");
            }

            tre.isError = block["is_error"].toBool(false);
            emit toolResult(tre);
        }
    }
}

// ── Result event (session turn complete) ───────────────────────────────

void ClaudeBridge::handleResultEvent(const QJsonObject &obj)
{
    double cost = obj["total_cost_usd"].toDouble();
    int turns = obj["num_turns"].toInt();
    emit sessionEnded(cost, turns);
}
