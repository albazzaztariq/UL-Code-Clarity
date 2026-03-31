#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QPoint>

// Generic LSP client — speaks JSON-RPC 2.0 over stdio to any language server.
// Handles: initialize, didOpen, didChange, completion, hover, gotoDefinition, diagnostics.

struct LspDiagnostic {
    int startLine;
    int startCol;
    int endLine;
    int endCol;
    int severity;          // 1=Error, 2=Warning, 3=Info, 4=Hint
    QString message;
    QString source;
};

struct LspCompletionItem {
    QString label;
    QString detail;
    QString insertText;
    int kind;              // 1=Text, 2=Method, 3=Function, 6=Variable, etc.
};

struct LspHoverResult {
    QString contents;      // markdown or plain text
};

struct LspLocation {
    QString uri;
    int line;
    int col;
};

class LspClient : public QObject {
    Q_OBJECT
public:
    explicit LspClient(QObject *parent = nullptr);
    ~LspClient();

    // Start a language server process.
    // command: "clangd", "pyright-langserver --stdio", etc.
    void start(const QString &command, const QStringList &args = {},
               const QString &rootUri = {});

    void stop();
    bool isRunning() const;

    // Lifecycle
    void initialize(const QString &rootUri);
    void shutdown();

    // Document sync
    void didOpen(const QString &uri, const QString &languageId,
                 const QString &text, int version = 1);
    void didChange(const QString &uri, const QString &text, int version);
    void didClose(const QString &uri);

    // Features
    void requestCompletion(const QString &uri, int line, int col);
    void requestHover(const QString &uri, int line, int col);
    void requestGotoDefinition(const QString &uri, int line, int col);

signals:
    void initialized();
    void diagnosticsReceived(const QString &uri, const QList<LspDiagnostic> &diags);
    void completionReceived(const QList<LspCompletionItem> &items);
    void hoverReceived(const LspHoverResult &result);
    void gotoDefinitionReceived(const LspLocation &location);
    void serverError(const QString &message);

private slots:
    void onReadyRead();
    void onProcessError(QProcess::ProcessError error);

private:
    void sendRequest(const QString &method, const QJsonObject &params);
    void sendNotification(const QString &method, const QJsonObject &params);
    void sendMessage(const QJsonObject &msg);
    void processMessage(const QJsonObject &msg);
    void handleResponse(int id, const QJsonObject &result);
    void handleNotification(const QString &method, const QJsonObject &params);

    QProcess *m_process = nullptr;
    QByteArray m_readBuffer;
    int m_nextId = 1;

    // Track pending request IDs -> method names
    QMap<int, QString> m_pendingRequests;
};
