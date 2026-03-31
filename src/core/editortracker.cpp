#include "core/editortracker.h"
#include "editor/editor.h"
#include <QTextBlock>
#include <QTextCursor>
#include <QFile>
#include <QFileInfo>
#include <QScrollBar>
#include <QDebug>

EditorTracker::EditorTracker(EditorWidget *editor, ClaudeBridge *bridge,
                             QObject *parent)
    : QObject(parent), m_editor(editor), m_bridge(bridge)
{
    connect(m_bridge, &ClaudeBridge::toolUseReady,
            this, &EditorTracker::onToolUseReady);
    connect(m_bridge, &ClaudeBridge::toolResult,
            this, &EditorTracker::onToolResult);

    // Clear highlight after a brief flash
    m_highlightTimer.setSingleShot(true);
    m_highlightTimer.setInterval(1500);
    connect(&m_highlightTimer, &QTimer::timeout, this, &EditorTracker::clearHighlight);
}

// ============================================================================
// Tool Event Dispatch
// ============================================================================

void EditorTracker::onToolUseReady(const ClaudeBridge::ToolUseEvent &event)
{
    QString name = event.name;

    if (name == "Edit")
        handleEdit(event);
    else if (name == "Write")
        handleWrite(event);
    else if (name == "Read")
        handleRead(event);
    else if (name == "Bash")
        handleBash(event);

    // Track tool ID -> file for result correlation
    QString filePath = event.input["file_path"].toString();
    if (!filePath.isEmpty())
        m_toolFileMap[event.id] = filePath;
}

void EditorTracker::onToolResult(const ClaudeBridge::ToolResultEvent &event)
{
    // After a tool completes, reload/open the file from disk
    QString filePath = m_toolFileMap.take(event.toolUseId);
    if (filePath.isEmpty()) return;

    // Always open the file (handles both new files from Write and existing from Edit)
    if (QFile::exists(filePath)) {
        int cursorPos = -1;
        QStringList openFiles = m_editor->openFilePaths();
        if (openFiles.contains(filePath))
            cursorPos = m_editor->codeEditor()->textCursor().position();

        m_editor->openFile(filePath);

        // Restore cursor if file was already open
        if (cursorPos >= 0) {
            QTextCursor cursor = m_editor->codeEditor()->textCursor();
            cursor.setPosition(qMin(cursorPos,
                m_editor->codeEditor()->document()->characterCount() - 1));
            m_editor->codeEditor()->setTextCursor(cursor);
        }
    }
}

// ============================================================================
// Tool Handlers
// ============================================================================

void EditorTracker::handleEdit(const ClaudeBridge::ToolUseEvent &event)
{
    QString filePath  = event.input["file_path"].toString();
    QString oldString = event.input["old_string"].toString();
    QString newString = event.input["new_string"].toString();

    if (filePath.isEmpty()) return;

    // Open the file in a tab
    m_editor->openFile(filePath);

    // Find the old_string in the editor buffer and scroll to it
    CodeEditor *ce = m_editor->codeEditor();
    QTextDocument *doc = ce->document();

    QTextCursor found = doc->find(oldString);
    if (!found.isNull()) {
        int startLine = found.blockNumber();
        int lineCount = newString.count('\n') + 1;

        // Scroll to the edit location
        scrollToLine(startLine);

        // Highlight the region being changed
        highlightLines(startLine, qMax(lineCount, oldString.count('\n') + 1),
                       QColor(255, 180, 50, 40));  // warm orange flash
    }

    // Changelog entry
    QString fileName = QFileInfo(filePath).fileName();
    int changeLines = newString.count('\n') + 1;
    QString entry = QString("Edit %1 (%2 lines changed)").arg(fileName).arg(changeLines);
    m_changelog.append(entry);
    emit fileModified(filePath, entry);
    emit changelogUpdated(m_changelog);
}

void EditorTracker::handleWrite(const ClaudeBridge::ToolUseEvent &event)
{
    QString filePath = event.input["file_path"].toString();
    QString content  = event.input["content"].toString();

    if (filePath.isEmpty()) return;

    // Open the new file — the tool_result will have written it to disk,
    // but we open it now for the visual effect. The file may not exist
    // yet (tool hasn't executed), so we'll reload on tool_result.
    // For now, just record the intent.

    // Changelog entry
    QString fileName = QFileInfo(filePath).fileName();
    int lineCount = content.count('\n') + 1;
    QString entry = QString("Create %1 (%2 lines)").arg(fileName).arg(lineCount);
    m_changelog.append(entry);
    emit fileModified(filePath, entry);
    emit changelogUpdated(m_changelog);
}

void EditorTracker::handleRead(const ClaudeBridge::ToolUseEvent &event)
{
    QString filePath = event.input["file_path"].toString();
    int offset = event.input["offset"].toInt(0);

    if (filePath.isEmpty()) return;

    // Open the file and scroll to where Claude is reading
    m_editor->openFile(filePath);

    if (offset > 0)
        scrollToLine(offset);

    // Brief blue highlight to show "Claude is reading here"
    int limit = event.input["limit"].toInt(30);
    highlightLines(offset, limit, QColor(80, 160, 255, 30));  // subtle blue
}

void EditorTracker::handleBash(const ClaudeBridge::ToolUseEvent &event)
{
    QString command = event.input["command"].toString();
    if (command.isEmpty()) return;

    // Changelog entry for significant commands
    QString entry = QString("Run: %1").arg(
        command.length() > 60 ? command.left(57) + "..." : command);
    m_changelog.append(entry);
    emit changelogUpdated(m_changelog);
}

// ============================================================================
// Editor Scroll & Highlight
// ============================================================================

void EditorTracker::scrollToLine(int lineNumber)
{
    CodeEditor *ce = m_editor->codeEditor();
    QTextBlock block = ce->document()->findBlockByNumber(qMax(0, lineNumber - 1));
    if (!block.isValid()) return;

    QTextCursor cursor(block);
    ce->setTextCursor(cursor);
    ce->centerCursor();
}

void EditorTracker::highlightLines(int startLine, int count, const QColor &color)
{
    CodeEditor *ce = m_editor->codeEditor();

    QList<QTextEdit::ExtraSelection> selections;
    for (int i = 0; i < count; ++i) {
        QTextBlock block = ce->document()->findBlockByNumber(startLine + i);
        if (!block.isValid()) break;

        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(color);
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.cursor = QTextCursor(block);
        sel.cursor.clearSelection();
        selections.append(sel);
    }

    ce->setExtraSelections(selections);
    m_highlightTimer.start();  // auto-clear after 1.5s
}

void EditorTracker::clearHighlight()
{
    CodeEditor *ce = m_editor->codeEditor();
    ce->setExtraSelections({});
}
