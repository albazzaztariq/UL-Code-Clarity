#pragma once

#include <QObject>
#include <QTimer>
#include "core/claudebridge.h"

class EditorWidget;
class CodeEditor;

// EditorTracker — watches ClaudeBridge tool events and drives the editor.
//
// When Claude edits/writes/reads a file:
//   - Opens the file in a tab (if not already open)
//   - Scrolls to the affected region
//   - Highlights the change briefly
//
// Also accumulates a changelog of all mutations for the session.

class EditorTracker : public QObject {
    Q_OBJECT
public:
    explicit EditorTracker(EditorWidget *editor, ClaudeBridge *bridge,
                           QObject *parent = nullptr);

    // Get the accumulated changelog entries for this session
    QStringList changelog() const { return m_changelog; }
    void clearChangelog() { m_changelog.clear(); }

signals:
    // Emitted when a file is modified by Claude — UI can show a notification
    void fileModified(const QString &filePath, const QString &description);

    // Emitted when changelog is updated
    void changelogUpdated(const QStringList &entries);

private slots:
    void onToolUseReady(const ClaudeBridge::ToolUseEvent &event);
    void onToolResult(const ClaudeBridge::ToolResultEvent &event);

private:
    void handleEdit(const ClaudeBridge::ToolUseEvent &event);
    void handleWrite(const ClaudeBridge::ToolUseEvent &event);
    void handleRead(const ClaudeBridge::ToolUseEvent &event);
    void handleBash(const ClaudeBridge::ToolUseEvent &event);

    // Scroll the editor to show a specific line, with a brief highlight
    void scrollToLine(int lineNumber);
    void highlightLines(int startLine, int count, const QColor &color);
    void clearHighlight();

    EditorWidget *m_editor;
    ClaudeBridge *m_bridge;
    QStringList m_changelog;
    QTimer m_highlightTimer;

    // Track which tool_use IDs map to which files (for result correlation)
    QMap<QString, QString> m_toolFileMap;
};
