#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QTabBar>
#include <QVBoxLayout>
#include <QMap>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>

class SyntaxHighlighter;

// ── Code Editor Pane (with line numbers) ────────────────────────────────
class CodeEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit CodeEditor(QWidget* parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent* event);
    int  lineNumberAreaWidth() const;
    void applyTheme(bool isDark);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect& rect, int dy);
    void highlightCurrentLine();

private:
    QWidget* m_lineNumberArea;
    bool m_isDark = true;
};

// ── Line Number Area Widget ─────────────────────────────────────────────
class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(CodeEditor* editor) : QWidget(editor), m_editor(editor) {}

    QSize sizeHint() const override {
        return QSize(m_editor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        m_editor->lineNumberAreaPaintEvent(event);
    }

private:
    CodeEditor* m_editor;
};

// ── Editor Widget (tabs + code editor + lang badge) ─────────────────────
class EditorWidget : public QWidget {
    Q_OBJECT

public:
    explicit EditorWidget(QWidget* parent = nullptr);

    void openFile(const QString& filePath);
    void newUntitled();
    void saveCurrentFile();
    void saveCurrentFileAs();
    void undo();
    void redo();

    QString currentLanguage() const;
    QString currentFilePath() const;
    // Returns content from the active tab (reads live editor state first)
    QString currentContent() const;
    CodeEditor* codeEditor() const { return m_codeEditor; }

    QStringList openFilePaths() const;
    int activeTabIndex() const { return m_currentTab; }
    void setActiveTab(int index);
    bool hasOpenFiles() const { return m_tabBar->count() > 0; }
    void showWelcome();
    void hideWelcome();
    void applyTheme(bool isDark);

    // Show/hide the "Clear Explanations" button in the toolbar
    void setExplainActive(bool active);

signals:
    void cursorPositionUpdated(int line, int col);
    void languageChanged(const QString& langDisplay);
    void fileOpened(const QString& filePath);
    void openFileRequested();
    void openFolderRequested();
    void explainRequested();
    void clearExplainRequested();
    void walkThroughRequested();

private:
    void switchToTab(int index);
    void closeTab(int index);
    void updateLanguageBadge();
    QString detectLanguage(const QString& filePath) const;
    QString languageDisplay(const QString& lang) const;
    void applyCloseButtonStyle(QPushButton* btn, bool isDark) const;
    QPushButton* makeCloseButton(int tabIdx);

    struct TabInfo {
        QString filePath;
        QString content;
        QString language;
        int cursorPos = 0;
    };

    QTabBar*          m_tabBar;
    CodeEditor*       m_codeEditor;
    SyntaxHighlighter* m_highlighter;
    QLabel*           m_langBadge;
    QPushButton*      m_clearExplainBtn = nullptr;
    QMap<int, TabInfo> m_tabs;  // tab index -> info
    int               m_currentTab = -1;

    // Welcome page (shown when no files open)
    QStackedWidget*   m_editorStack;
    QWidget*          m_welcomePage;
    QString           m_welcomeLang = "python";
    void              setWelcomeCode(const QString& lang);
    bool              m_isDark = true;
};
