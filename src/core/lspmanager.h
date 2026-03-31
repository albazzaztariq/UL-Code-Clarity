#pragma once

#include <QObject>
#include <QMap>
#include "core/lspclient.h"

class EditorWidget;
class CodeEditor;

// LspManager — manages per-language LSP clients and wires them to the editor.
// Auto-starts the appropriate language server when a file is opened.
// Provides: diagnostics (squigglies), completion, hover, goto definition.

class LspManager : public QObject {
    Q_OBJECT
public:
    explicit LspManager(EditorWidget *editor, QObject *parent = nullptr);
    ~LspManager();

    // Called when a file is opened/switched to
    void fileOpened(const QString &filePath, const QString &language,
                    const QString &content);

    // Called when file content changes
    void fileChanged(const QString &filePath, const QString &content);

    // Called when file is closed
    void fileClosed(const QString &filePath);

    // Trigger completion at current cursor position
    void requestCompletion(const QString &filePath, int line, int col);

    // Trigger hover info at position
    void requestHover(const QString &filePath, int line, int col);

    // Trigger goto definition at position
    void requestGotoDefinition(const QString &filePath, int line, int col);

    // Set the workspace root (for LSP rootUri)
    void setWorkspaceRoot(const QString &path);

signals:
    void diagnosticsReady(const QString &filePath, const QList<LspDiagnostic> &diags);
    void completionReady(const QList<LspCompletionItem> &items);
    void hoverReady(const LspHoverResult &result);
    void gotoDefinitionReady(const LspLocation &location);

private:
    LspClient* clientForLanguage(const QString &language);
    LspClient* ensureClient(const QString &language);
    QString languageId(const QString &language) const;
    QString fileUri(const QString &filePath) const;

    // Server command + args for each language
    struct ServerConfig {
        QString command;
        QStringList args;
    };
    static ServerConfig serverForLanguage(const QString &langId);

    EditorWidget *m_editor;
    QString m_workspaceRoot;
    QMap<QString, LspClient*> m_clients;  // language -> client
    QMap<QString, int> m_versions;         // uri -> document version
};
