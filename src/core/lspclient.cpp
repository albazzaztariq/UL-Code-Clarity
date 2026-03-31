#include "core/lspclient.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QUrl>

LspClient::LspClient(QObject *parent) : QObject(parent) {}

LspClient::~LspClient() { stop(); }

// ── Process Management ─────────────────────────────────────────────────

void LspClient::start(const QString &command, const QStringList &args,
                       const QString &rootUri)
{
    if (m_process) stop();

    m_process = new QProcess(this);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &LspClient::onReadyRead);
    connect(m_process, &QProcess::errorOccurred, this, &LspClient::onProcessError);
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        QString err = QString::fromUtf8(m_process->readAllStandardError());
        if (!err.trimmed().isEmpty())
            qDebug() << "[LSP stderr]" << err.trimmed();
    });

    qDebug() << "[LSP] Starting:" << command << args;
    m_process->start(command, args);

    if (!m_process->waitForStarted(5000)) {
        emit serverError("Failed to start LSP server: " + command + " " + m_process->errorString());
        m_process->deleteLater();
        m_process = nullptr;
        return;
    }

    initialize(rootUri);
}

void LspClient::stop()
{
    if (!m_process) return;
    if (m_process->state() == QProcess::Running) {
        shutdown();
        if (!m_process->waitForFinished(3000))
            m_process->kill();
    }
    m_process->deleteLater();
    m_process = nullptr;
    m_readBuffer.clear();
    m_pendingRequests.clear();
    m_nextId = 1;
}

bool LspClient::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}

// ── LSP Lifecycle ──────────────────────────────────────────────────────

void LspClient::initialize(const QString &rootUri)
{
    QJsonObject caps;
    caps["textDocument"] = QJsonObject{
        {"completion", QJsonObject{
            {"completionItem", QJsonObject{{"snippetSupport", false}}}
        }},
        {"hover", QJsonObject{{"contentFormat", QJsonArray{"plaintext", "markdown"}}}},
        {"publishDiagnostics", QJsonObject{{"relatedInformation", true}}}
    };

    QJsonObject params;
    params["processId"] = QJsonValue((int)QCoreApplication::applicationPid());
    params["rootUri"] = rootUri.isEmpty() ? QJsonValue() : QJsonValue(rootUri);
    params["capabilities"] = caps;

    sendRequest("initialize", params);
}

void LspClient::shutdown()
{
    sendRequest("shutdown", QJsonObject());
    sendNotification("exit", QJsonObject());
}

// ── Document Sync ──────────────────────────────────────────────────────

void LspClient::didOpen(const QString &uri, const QString &languageId,
                         const QString &text, int version)
{
    QJsonObject textDoc;
    textDoc["uri"] = uri;
    textDoc["languageId"] = languageId;
    textDoc["version"] = version;
    textDoc["text"] = text;

    QJsonObject params;
    params["textDocument"] = textDoc;
    sendNotification("textDocument/didOpen", params);
}

void LspClient::didChange(const QString &uri, const QString &text, int version)
{
    QJsonObject params;
    params["textDocument"] = QJsonObject{{"uri", uri}, {"version", version}};
    params["contentChanges"] = QJsonArray{QJsonObject{{"text", text}}};
    sendNotification("textDocument/didChange", params);
}

void LspClient::didClose(const QString &uri)
{
    QJsonObject params;
    params["textDocument"] = QJsonObject{{"uri", uri}};
    sendNotification("textDocument/didClose", params);
}

// ── Feature Requests ───────────────────────────────────────────────────

void LspClient::requestCompletion(const QString &uri, int line, int col)
{
    QJsonObject params;
    params["textDocument"] = QJsonObject{{"uri", uri}};
    params["position"] = QJsonObject{{"line", line}, {"character", col}};
    sendRequest("textDocument/completion", params);
}

void LspClient::requestHover(const QString &uri, int line, int col)
{
    QJsonObject params;
    params["textDocument"] = QJsonObject{{"uri", uri}};
    params["position"] = QJsonObject{{"line", line}, {"character", col}};
    sendRequest("textDocument/hover", params);
}

void LspClient::requestGotoDefinition(const QString &uri, int line, int col)
{
    QJsonObject params;
    params["textDocument"] = QJsonObject{{"uri", uri}};
    params["position"] = QJsonObject{{"line", line}, {"character", col}};
    sendRequest("textDocument/definition", params);
}

// ── JSON-RPC Transport ─────────────────────────────────────────────────

void LspClient::sendRequest(const QString &method, const QJsonObject &params)
{
    int id = m_nextId++;
    m_pendingRequests[id] = method;

    QJsonObject msg;
    msg["jsonrpc"] = "2.0";
    msg["id"] = id;
    msg["method"] = method;
    msg["params"] = params;
    sendMessage(msg);
}

void LspClient::sendNotification(const QString &method, const QJsonObject &params)
{
    QJsonObject msg;
    msg["jsonrpc"] = "2.0";
    msg["method"] = method;
    msg["params"] = params;
    sendMessage(msg);
}

void LspClient::sendMessage(const QJsonObject &msg)
{
    if (!m_process || m_process->state() != QProcess::Running) return;

    QByteArray body = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    QByteArray header = "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n";
    m_process->write(header + body);
}

// ── Reading Responses ──────────────────────────────────────────────────

void LspClient::onReadyRead()
{
    m_readBuffer += m_process->readAllStandardOutput();

    // Parse LSP messages: "Content-Length: N\r\n\r\n{...json...}"
    while (true) {
        int headerEnd = m_readBuffer.indexOf("\r\n\r\n");
        if (headerEnd < 0) break;

        // Parse Content-Length
        QByteArray headerBlock = m_readBuffer.left(headerEnd);
        int contentLength = -1;
        for (const QByteArray &line : headerBlock.split('\n')) {
            QByteArray trimmed = line.trimmed();
            if (trimmed.startsWith("Content-Length:")) {
                contentLength = trimmed.mid(15).trimmed().toInt();
                break;
            }
        }

        if (contentLength < 0) {
            // Malformed header — skip
            m_readBuffer = m_readBuffer.mid(headerEnd + 4);
            continue;
        }

        int bodyStart = headerEnd + 4;
        if (m_readBuffer.size() < bodyStart + contentLength)
            break;  // Wait for more data

        QByteArray body = m_readBuffer.mid(bodyStart, contentLength);
        m_readBuffer = m_readBuffer.mid(bodyStart + contentLength);

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(body, &err);
        if (err.error == QJsonParseError::NoError && doc.isObject())
            processMessage(doc.object());
    }
}

void LspClient::onProcessError(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    if (m_process)
        emit serverError("LSP process error: " + m_process->errorString());
}

// ── Message Dispatch ───────────────────────────────────────────────────

void LspClient::processMessage(const QJsonObject &msg)
{
    if (msg.contains("id") && msg.contains("result")) {
        // Response to our request
        int id = msg["id"].toInt();
        handleResponse(id, msg["result"].toObject());
    }
    else if (msg.contains("id") && msg.contains("error")) {
        int id = msg["id"].toInt();
        QString method = m_pendingRequests.take(id);
        QJsonObject error = msg["error"].toObject();
        qWarning() << "[LSP] Error in" << method << ":"
                    << error["message"].toString();
    }
    else if (msg.contains("method")) {
        // Server notification
        handleNotification(msg["method"].toString(),
                           msg["params"].toObject());
    }
}

void LspClient::handleResponse(int id, const QJsonObject &result)
{
    QString method = m_pendingRequests.take(id);

    if (method == "initialize") {
        // Send initialized notification
        sendNotification("initialized", QJsonObject());
        emit initialized();
    }
    else if (method == "textDocument/completion") {
        QList<LspCompletionItem> items;
        // Completion can be an array or an object with "items"
        QJsonArray arr = result.contains("items")
            ? result["items"].toArray()
            : QJsonArray();
        // Handle case where result itself is the array (via the parent)
        if (arr.isEmpty()) {
            QJsonDocument doc(result);
            // Try treating the whole result as having items
        }
        for (const auto &v : arr) {
            QJsonObject obj = v.toObject();
            LspCompletionItem item;
            item.label = obj["label"].toString();
            item.detail = obj["detail"].toString();
            item.insertText = obj["insertText"].toString();
            if (item.insertText.isEmpty()) item.insertText = item.label;
            item.kind = obj["kind"].toInt();
            items.append(item);
        }
        emit completionReceived(items);
    }
    else if (method == "textDocument/hover") {
        LspHoverResult hover;
        QJsonValue contents = result["contents"];
        if (contents.isString()) {
            hover.contents = contents.toString();
        } else if (contents.isObject()) {
            hover.contents = contents.toObject()["value"].toString();
        } else if (contents.isArray()) {
            QStringList parts;
            for (const auto &c : contents.toArray()) {
                if (c.isString()) parts << c.toString();
                else if (c.isObject()) parts << c.toObject()["value"].toString();
            }
            hover.contents = parts.join("\n");
        }
        emit hoverReceived(hover);
    }
    else if (method == "textDocument/definition") {
        LspLocation loc;
        // Can be a single Location or an array
        QJsonObject locObj = result;
        if (result.contains("uri")) {
            locObj = result;
        }
        loc.uri = locObj["uri"].toString();
        QJsonObject range = locObj["range"].toObject();
        QJsonObject start = range["start"].toObject();
        loc.line = start["line"].toInt();
        loc.col = start["character"].toInt();
        emit gotoDefinitionReceived(loc);
    }
}

void LspClient::handleNotification(const QString &method, const QJsonObject &params)
{
    if (method == "textDocument/publishDiagnostics") {
        QString uri = params["uri"].toString();
        QList<LspDiagnostic> diags;
        for (const auto &v : params["diagnostics"].toArray()) {
            QJsonObject d = v.toObject();
            QJsonObject range = d["range"].toObject();
            QJsonObject start = range["start"].toObject();
            QJsonObject end = range["end"].toObject();

            LspDiagnostic diag;
            diag.startLine = start["line"].toInt();
            diag.startCol = start["character"].toInt();
            diag.endLine = end["line"].toInt();
            diag.endCol = end["character"].toInt();
            diag.severity = d["severity"].toInt(1);
            diag.message = d["message"].toString();
            diag.source = d["source"].toString();
            diags.append(diag);
        }
        emit diagnosticsReceived(uri, diags);
    }
}
