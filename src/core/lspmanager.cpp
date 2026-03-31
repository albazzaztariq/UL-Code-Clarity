#include "core/lspmanager.h"
#include "editor/editor.h"
#include <QUrl>
#include <QDir>
#include <QDebug>
#include <QProcess>

LspManager::LspManager(EditorWidget *editor, QObject *parent)
    : QObject(parent), m_editor(editor)
{
}

LspManager::~LspManager()
{
    for (auto *client : m_clients)
        client->stop();
}

void LspManager::setWorkspaceRoot(const QString &path)
{
    m_workspaceRoot = path;
}

// ── File Events ────────────────────────────────────────────────────────

void LspManager::fileOpened(const QString &filePath, const QString &language,
                             const QString &content)
{
    QString langId = languageId(language);
    if (langId.isEmpty()) return;

    LspClient *client = ensureClient(langId);
    if (!client || !client->isRunning()) return;

    QString uri = fileUri(filePath);
    m_versions[uri] = 1;
    client->didOpen(uri, langId, content, 1);
}

void LspManager::fileChanged(const QString &filePath, const QString &content)
{
    QString uri = fileUri(filePath);
    if (!m_versions.contains(uri)) return;

    int version = ++m_versions[uri];

    // Find the client for this file
    QString lang = languageId(m_editor->currentLanguage());
    LspClient *client = clientForLanguage(lang);
    if (!client || !client->isRunning()) return;

    client->didChange(uri, content, version);
}

void LspManager::fileClosed(const QString &filePath)
{
    QString uri = fileUri(filePath);
    m_versions.remove(uri);

    QString lang = languageId(m_editor->currentLanguage());
    LspClient *client = clientForLanguage(lang);
    if (client && client->isRunning())
        client->didClose(uri);
}

// ── Feature Requests ───────────────────────────────────────────────────

void LspManager::requestCompletion(const QString &filePath, int line, int col)
{
    QString lang = languageId(m_editor->currentLanguage());
    LspClient *client = clientForLanguage(lang);
    if (client && client->isRunning())
        client->requestCompletion(fileUri(filePath), line, col);
}

void LspManager::requestHover(const QString &filePath, int line, int col)
{
    QString lang = languageId(m_editor->currentLanguage());
    LspClient *client = clientForLanguage(lang);
    if (client && client->isRunning())
        client->requestHover(fileUri(filePath), line, col);
}

void LspManager::requestGotoDefinition(const QString &filePath, int line, int col)
{
    QString lang = languageId(m_editor->currentLanguage());
    LspClient *client = clientForLanguage(lang);
    if (client && client->isRunning())
        client->requestGotoDefinition(fileUri(filePath), line, col);
}

// ── Client Management ──────────────────────────────────────────────────

LspClient* LspManager::clientForLanguage(const QString &language)
{
    return m_clients.value(language, nullptr);
}

LspClient* LspManager::ensureClient(const QString &langId)
{
    if (m_clients.contains(langId))
        return m_clients[langId];

    ServerConfig cfg = serverForLanguage(langId);
    if (cfg.command.isEmpty()) {
        qDebug() << "[LSP] No server configured for language:" << langId;
        return nullptr;
    }

    // Verify the server binary exists
    QProcess check;
    check.start(cfg.command, {"--version"});
    if (!check.waitForFinished(3000) || check.exitCode() != 0) {
        // Try --help as fallback (some servers don't support --version)
        check.start(cfg.command, {"--help"});
        if (!check.waitForFinished(3000)) {
            qDebug() << "[LSP] Server not found:" << cfg.command;
            return nullptr;
        }
    }

    auto *client = new LspClient(this);
    m_clients[langId] = client;

    // Wire signals
    connect(client, &LspClient::diagnosticsReceived, this,
        [this](const QString &uri, const QList<LspDiagnostic> &diags) {
        // Convert URI back to file path
        QString path = QUrl(uri).toLocalFile();
        emit diagnosticsReady(path, diags);
    });
    connect(client, &LspClient::completionReceived, this, &LspManager::completionReady);
    connect(client, &LspClient::hoverReceived, this, &LspManager::hoverReady);
    connect(client, &LspClient::gotoDefinitionReceived, this, &LspManager::gotoDefinitionReady);
    connect(client, &LspClient::serverError, this, [langId](const QString &msg) {
        qWarning() << "[LSP]" << langId << "error:" << msg;
    });

    QString rootUri;
    if (!m_workspaceRoot.isEmpty())
        rootUri = QUrl::fromLocalFile(m_workspaceRoot).toString();

    client->start(cfg.command, cfg.args, rootUri);
    return client;
}

// ── Language Mapping ───────────────────────────────────────────────────

QString LspManager::languageId(const QString &language) const
{
    QString lower = language.toLower();
    if (lower == "python" || lower == "py") return "python";
    if (lower == "c" || lower == "cpp" || lower == "c++") return "c";
    if (lower == "javascript" || lower == "js") return "javascript";
    if (lower == "typescript" || lower == "ts") return "typescript";
    if (lower == "unilogic" || lower == "ul") return "unilogic";
    return {};
}

QString LspManager::fileUri(const QString &filePath) const
{
    return QUrl::fromLocalFile(filePath).toString();
}

LspManager::ServerConfig LspManager::serverForLanguage(const QString &langId)
{
    if (langId == "python")
        return {"pyright-langserver", {"--stdio"}};
    if (langId == "c")
        return {"clangd", {"--background-index"}};
    if (langId == "javascript" || langId == "typescript")
        return {"typescript-language-server", {"--stdio"}};
    if (langId == "unilogic")
        return {"ul-lsp", {"--stdio"}};  // UniLogic LSP if available
    return {};
}
