#include "editor/editor.h"
#include "editor/highlighter.h"
#include "core/theme.h"

#include <QPainter>
#include <QTextBlock>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

// ═══════════════════════════════════════════════════════════════════════
// CodeEditor — QPlainTextEdit with line numbers
// ═══════════════════════════════════════════════════════════════════════

CodeEditor::CodeEditor(QWidget* parent)
    : QPlainTextEdit(parent)
{
    m_lineNumberArea = new LineNumberArea(this);

    setStyleSheet(
        "QPlainTextEdit {"
        "  background: #1e1e2e;"
        "  color: #cdd6f4;"
        "  font-family: 'Cascadia Code', 'Consolas', monospace;"
        "  font-size: 12px;"
        "  border: none;"
        "  selection-background-color: #45475a;"
        "  selection-color: #cdd6f4;"
        "}"
    );

    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * 4);
    setLineWrapMode(QPlainTextEdit::NoWrap);

    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

int CodeEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    return 12 + fontMetrics().horizontalAdvance('9') * qMax(digits, 2) + 8;
}

void CodeEditor::updateLineNumberAreaWidth(int)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect& rect, int dy)
{
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(),
                                        lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    QTextEdit::ExtraSelection selection;
    // Very subtle highlight — just slightly lighter/darker than the editor background
    selection.format.setBackground(m_isDark ? QColor(0x26, 0x26, 0x3a) : QColor(0xf0, 0xf4, 0xff));
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    extraSelections.append(selection);

    setExtraSelections(extraSelections);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent* event)
{
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), m_isDark ? QColor(0x2a, 0x2a, 0x3c) : QColor(0xf0, 0xf0, 0xf0));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    QFont font("Cascadia Code", 9);
    painter.setFont(font);

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            bool isCurrent = (textCursor().blockNumber() == blockNumber);
            if (m_isDark) {
                painter.setPen(isCurrent ? QColor(0xcd, 0xd6, 0xf4) : QColor(0x6c, 0x70, 0x86));
            } else {
                painter.setPen(isCurrent ? QColor(0x1e, 0x1e, 0x2e) : QColor(0x99, 0x99, 0x99));
            }
            painter.drawText(0, top, m_lineNumberArea->width() - 8,
                             fontMetrics().height(),
                             Qt::AlignRight | Qt::AlignVCenter, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void CodeEditor::applyTheme(bool isDark)
{
    m_isDark = isDark;
    if (isDark) {
        setStyleSheet(
            "QPlainTextEdit {"
            "  background: #1e1e2e;"
            "  color: #cdd6f4;"
            "  font-family: 'Cascadia Code', 'Consolas', monospace;"
            "  font-size: 12px;"
            "  border: none;"
            "  selection-background-color: #45475a;"
            "  selection-color: #cdd6f4;"
            "}"
        );
    } else {
        setStyleSheet(
            "QPlainTextEdit {"
            "  background: #ffffff;"
            "  color: #1e1e2e;"
            "  font-family: 'Cascadia Code', 'Consolas', monospace;"
            "  font-size: 12px;"
            "  border: none;"
            "  selection-background-color: #bfdbfe;"
            "  selection-color: #1e1e2e;"
            "}"
        );
    }
    highlightCurrentLine();
    m_lineNumberArea->update();
}

// ═══════════════════════════════════════════════════════════════════════
// EditorWidget — tab bar + code editor + language badge
// ═══════════════════════════════════════════════════════════════════════

EditorWidget::EditorWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Tab bar row
    auto* tabRow = new QWidget;
    auto* tabLayout = new QHBoxLayout(tabRow);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(0);
    tabRow->setStyleSheet("background: #2a2a3c; border-bottom: 1px solid #313244;");

    m_tabBar = new QTabBar;
    m_tabBar->setTabsClosable(false);  // we place our own styled close buttons
    m_tabBar->setMovable(true);
    m_tabBar->setExpanding(false);
    tabLayout->addWidget(m_tabBar);

    tabLayout->addStretch();

    // Language badge — hidden until a file is opened
    m_langBadge = new QLabel;
    m_langBadge->setStyleSheet(
        "QLabel { color: #94e2d5; background: rgba(148,226,213,0.1);"
        " padding: 2px 8px; border-radius: 4px; font-size: 10px; font-weight: 600;"
        " margin-right: 4px; }");
    m_langBadge->hide();
    tabLayout->addWidget(m_langBadge);

    // Explain button
    auto* explainBtn = new QPushButton("Explain");
    explainBtn->setFixedHeight(22);
    explainBtn->setCursor(Qt::PointingHandCursor);
    explainBtn->setToolTip("Explain the current code in plain language");
    explainBtn->setStyleSheet(
        "QPushButton { background: #f9e2af; color: #1e1e2e; font-weight: 600;"
        " font-size: 10px; padding: 0 10px; border-radius: 4px; border: none; margin-right: 4px; }"
        "QPushButton:hover { background: #fae3b0; }");
    tabLayout->addWidget(explainBtn);

    // Walk Me Through button
    auto* walkBtn = new QPushButton("Walk Me Through This Code");
    walkBtn->setFixedHeight(22);
    walkBtn->setCursor(Qt::PointingHandCursor);
    walkBtn->setToolTip("Get a step-by-step walkthrough of this code");
    walkBtn->setStyleSheet(
        "QPushButton { background: #f9e2af; color: #1e1e2e; font-weight: 600;"
        " font-size: 10px; padding: 0 10px; border-radius: 4px; border: none; margin-right: 8px; }"
        "QPushButton:hover { background: #fae3b0; }");
    tabLayout->addWidget(walkBtn);

    // Clear Explanations button — hidden until Explain is active
    m_clearExplainBtn = new QPushButton("Clear Explanations");
    m_clearExplainBtn->setFixedHeight(22);
    m_clearExplainBtn->setCursor(Qt::PointingHandCursor);
    m_clearExplainBtn->setToolTip("Remove inline comments added by Explain");
    m_clearExplainBtn->setStyleSheet(
        "QPushButton { background: #f38ba8; color: #1e1e2e; font-weight: 600;"
        " font-size: 10px; padding: 0 10px; border-radius: 4px; border: none; margin-right: 8px; }"
        "QPushButton:hover { background: #f5a0b5; }");
    m_clearExplainBtn->hide();
    tabLayout->addWidget(m_clearExplainBtn);

    layout->addWidget(tabRow);

    // Stacked widget: welcome page (0) vs code editor (1)
    m_editorStack = new QStackedWidget;

    // ── Welcome page (index 0) ──
    m_welcomePage = new QWidget;
    m_welcomePage->setStyleSheet("background: #1e1e2e;");
    auto* welcomeLayout = new QVBoxLayout(m_welcomePage);
    welcomeLayout->setContentsMargins(40, 40, 40, 40);
    welcomeLayout->setSpacing(16);

    auto* welcomeTitle = new QLabel("Welcome to Code Clarity");
    welcomeTitle->setStyleSheet(
        "QLabel { color: #cdd6f4; font-size: 18px; font-weight: 700; background: transparent; }");
    welcomeTitle->setAlignment(Qt::AlignCenter);
    welcomeLayout->addWidget(welcomeTitle);

    auto* welcomeSubtitle = new QLabel("Open a workspace or try the example below");
    welcomeSubtitle->setStyleSheet(
        "QLabel { color: #a6adc8; font-size: 12px; background: transparent; }");
    welcomeSubtitle->setAlignment(Qt::AlignCenter);
    welcomeLayout->addWidget(welcomeSubtitle);

    welcomeLayout->addSpacing(8);

    auto* codeBlock = new QPlainTextEdit;
    codeBlock->setReadOnly(true);
    codeBlock->setMaximumHeight(180);
    codeBlock->setStyleSheet(
        "QPlainTextEdit {"
        "  background: #2a2a3c; color: #cdd6f4; border: 1px solid #45475a;"
        "  border-radius: 6px; padding: 12px;"
        "  font-family: 'Cascadia Code', 'Consolas', monospace;"
        "  font-size: 13px;"
        "}");
    codeBlock->setObjectName("welcomeCodeBlock");
    welcomeLayout->addWidget(codeBlock);

    // Open buttons row
    auto* openBtnRow = new QWidget;
    openBtnRow->setStyleSheet("background: transparent;");
    auto* openBtnLayout = new QHBoxLayout(openBtnRow);
    openBtnLayout->setContentsMargins(0, 0, 0, 0);
    openBtnLayout->setSpacing(8);
    openBtnLayout->addStretch();

    auto* openFileBtn = new QPushButton("Open File");
    openFileBtn->setStyleSheet(
        "QPushButton { color: #cdd6f4; background: #45475a;"
        "  border: 1px solid #585b70; border-radius: 4px; padding: 6px 16px;"
        "  font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { background: #585b70; }");
    openFileBtn->setCursor(Qt::PointingHandCursor);
    openBtnLayout->addWidget(openFileBtn);

    auto* openFolderBtn = new QPushButton("Open a Folder to View All Files Inside");
    openFolderBtn->setStyleSheet(
        "QPushButton { color: #cdd6f4; background: #45475a;"
        "  border: 1px solid #585b70; border-radius: 4px; padding: 6px 16px;"
        "  font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { background: #585b70; }");
    openFolderBtn->setCursor(Qt::PointingHandCursor);
    openBtnLayout->addWidget(openFolderBtn);

    openBtnLayout->addStretch();
    welcomeLayout->addWidget(openBtnRow);

    welcomeLayout->addSpacing(8);

    // Language switch buttons
    auto* switchRow = new QWidget;
    switchRow->setStyleSheet("background: transparent;");
    auto* switchBtnLayout = new QHBoxLayout(switchRow);
    switchBtnLayout->setContentsMargins(0, 0, 0, 0);
    switchBtnLayout->setSpacing(8);
    switchBtnLayout->addStretch();

    auto* pyBtn = new QPushButton("Python");
    pyBtn->setStyleSheet(
        "QPushButton { color: #94e2d5; background: rgba(148,226,213,0.15);"
        "  border: 1px solid #94e2d5; border-radius: 4px; padding: 4px 14px;"
        "  font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { background: rgba(148,226,213,0.3); }");
    pyBtn->setCursor(Qt::PointingHandCursor);
    switchBtnLayout->addWidget(pyBtn);

    auto* cBtn = new QPushButton("Switch to C");
    cBtn->setStyleSheet(
        "QPushButton { color: #fab387; background: rgba(250,179,135,0.15);"
        "  border: 1px solid #fab387; border-radius: 4px; padding: 4px 14px;"
        "  font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { background: rgba(250,179,135,0.3); }");
    cBtn->setCursor(Qt::PointingHandCursor);
    switchBtnLayout->addWidget(cBtn);

    auto* ulBtn = new QPushButton("Switch to UniLogic");
    ulBtn->setStyleSheet(
        "QPushButton { color: #89b4fa; background: rgba(137,180,250,0.15);"
        "  border: 1px solid #89b4fa; border-radius: 4px; padding: 4px 14px;"
        "  font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { background: rgba(137,180,250,0.3); }");
    ulBtn->setCursor(Qt::PointingHandCursor);
    switchBtnLayout->addWidget(ulBtn);

    switchBtnLayout->addStretch();
    welcomeLayout->addWidget(switchRow);

    welcomeLayout->addStretch();
    m_editorStack->addWidget(m_welcomePage);  // index 0

    // ── Code editor (index 1) ──
    m_codeEditor = new CodeEditor;
    m_editorStack->addWidget(m_codeEditor);  // index 1

    layout->addWidget(m_editorStack, 1);

    // Syntax highlighter
    m_highlighter = new SyntaxHighlighter(m_codeEditor->document());

    // Set initial welcome code
    setWelcomeCode("python");

    // Welcome button connections
    connect(pyBtn, &QPushButton::clicked, this, [this]() { setWelcomeCode("python"); });
    connect(cBtn, &QPushButton::clicked, this, [this]() { setWelcomeCode("c"); });
    connect(ulBtn, &QPushButton::clicked, this, [this]() { setWelcomeCode("ul"); });
    connect(openFileBtn, &QPushButton::clicked, this, &EditorWidget::openFileRequested);
    connect(openFolderBtn, &QPushButton::clicked, this, &EditorWidget::openFolderRequested);

    // Toolbar button connections
    connect(explainBtn, &QPushButton::clicked, this, &EditorWidget::explainRequested);
    connect(walkBtn, &QPushButton::clicked, this, &EditorWidget::walkThroughRequested);
    connect(m_clearExplainBtn, &QPushButton::clicked, this, &EditorWidget::clearExplainRequested);

    // Connections
    connect(m_tabBar, &QTabBar::currentChanged, this, &EditorWidget::switchToTab);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &EditorWidget::closeTab);

    connect(m_codeEditor, &QPlainTextEdit::cursorPositionChanged, this, [this]() {
        QTextCursor cursor = m_codeEditor->textCursor();
        int line = cursor.blockNumber() + 1;
        int col = cursor.columnNumber() + 1;
        emit cursorPositionUpdated(line, col);
    });

    // Start with welcome page visible
    m_editorStack->setCurrentIndex(0);
}

void EditorWidget::openFile(const QString& filePath)
{
    // Check if already open
    for (auto it = m_tabs.begin(); it != m_tabs.end(); ++it) {
        if (it->filePath == filePath) {
            m_tabBar->setCurrentIndex(it.key());
            return;
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QByteArray raw = file.readAll();
    file.close();

    // If readAll() returned nothing, the file may be a cloud-only OneDrive placeholder.
    // Fall back to QTextStream which can handle some edge cases.
    if (raw.isEmpty() && QFileInfo(filePath).size() > 0) {
        QFile f2(filePath);
        if (f2.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream ts(&f2);
            QString streamed = ts.readAll();
            f2.close();
            if (!streamed.isEmpty())
                raw = streamed.toUtf8();
        }
    }

    // Decode as UTF-8, fall back to Latin-1 for binary-ish files
    QString content;
    if (raw.isEmpty() && QFileInfo(filePath).size() > 0) {
        // Still empty after both attempts — likely a cloud-only OneDrive placeholder
        content = QString(
            "// Cannot read file — it may be a cloud-only OneDrive file.\n"
            "// Right-click the file in File Explorer and choose\n"
            "// \"Always keep on this device\", then reopen it here."
        );
    } else {
        content = QString::fromUtf8(raw);
        if (content.contains(QChar(0xFFFD))) {
            content = QString::fromLatin1(raw);
        }
    }

    QFileInfo fi(filePath);
    QString lang = detectLanguage(filePath);

    // Populate tab data BEFORE addTab so switchToTab sees the data immediately
    int idx = m_tabBar->count();
    m_tabs[idx] = { filePath, content, lang, 0 };

    m_tabBar->blockSignals(true);
    int addedIdx = m_tabBar->addTab(fi.fileName());
    m_tabBar->setTabButton(addedIdx, QTabBar::RightSide, makeCloseButton(addedIdx));
    m_tabBar->blockSignals(false);
    Q_UNUSED(addedIdx);

    // Switch from welcome page to editor BEFORE switching tab so
    // the code editor is visible when content is set
    hideWelcome();

    m_tabBar->setCurrentIndex(idx);

    emit fileOpened(filePath);
}

void EditorWidget::saveCurrentFile()
{
    if (m_currentTab < 0 || !m_tabs.contains(m_currentTab))
        return;

    auto& tab = m_tabs[m_currentTab];
    tab.content = m_codeEditor->toPlainText();

    QFile file(tab.filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << tab.content;
        file.close();
    }
}

void EditorWidget::undo() { m_codeEditor->undo(); }
void EditorWidget::redo() { m_codeEditor->redo(); }

void EditorWidget::setExplainActive(bool active)
{
    if (m_clearExplainBtn)
        m_clearExplainBtn->setVisible(active);
}

QString EditorWidget::currentLanguage() const
{
    if (m_currentTab >= 0 && m_tabs.contains(m_currentTab))
        return m_tabs[m_currentTab].language;
    return {};
}

QString EditorWidget::currentFilePath() const
{
    if (m_currentTab >= 0 && m_tabs.contains(m_currentTab))
        return m_tabs[m_currentTab].filePath;
    return {};
}

QString EditorWidget::currentContent() const
{
    // Always read live from the code editor widget (it IS the active tab's content)
    return m_codeEditor->toPlainText();
}

QStringList EditorWidget::openFilePaths() const
{
    QStringList paths;
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabs.contains(i))
            paths << m_tabs[i].filePath;
    }
    return paths;
}

void EditorWidget::setActiveTab(int index)
{
    if (index >= 0 && index < m_tabBar->count())
        m_tabBar->setCurrentIndex(index);
}

void EditorWidget::switchToTab(int index)
{
    // Save current tab state
    if (m_currentTab >= 0 && m_tabs.contains(m_currentTab)) {
        m_tabs[m_currentTab].content = m_codeEditor->toPlainText();
        m_tabs[m_currentTab].cursorPos = m_codeEditor->textCursor().position();
    }

    m_currentTab = index;

    if (index >= 0 && m_tabs.contains(index)) {
        auto& tab = m_tabs[index];
        m_codeEditor->setPlainText(tab.content);

        QTextCursor cursor = m_codeEditor->textCursor();
        cursor.setPosition(qMin(tab.cursorPos, m_codeEditor->document()->characterCount() - 1));
        m_codeEditor->setTextCursor(cursor);

        m_highlighter->setLanguage(tab.language);
        updateLanguageBadge();

        emit languageChanged(languageDisplay(tab.language));
    }
}

void EditorWidget::closeTab(int index)
{
    if (m_tabs.contains(index))
        m_tabs.remove(index);
    m_tabBar->removeTab(index);

    // Re-key tabs after removal
    QMap<int, TabInfo> newTabs;
    for (auto it = m_tabs.begin(); it != m_tabs.end(); ++it) {
        int key = it.key();
        if (key > index) key--;
        newTabs[key] = it.value();
    }
    m_tabs = newTabs;

    if (m_tabs.isEmpty()) {
        m_langBadge->hide();
        m_currentTab = -1;
        showWelcome();
        emit languageChanged(QString());
    }
}

void EditorWidget::updateLanguageBadge()
{
    if (m_currentTab < 0 || !m_tabs.contains(m_currentTab)) {
        m_langBadge->hide();
        return;
    }

    const QString& lang = m_tabs[m_currentTab].language;
    QString display = languageDisplay(lang);
    m_langBadge->setText(display);
    m_langBadge->show();

    if (lang == "python") {
        m_langBadge->setStyleSheet(
            "QLabel { color: #94e2d5; background: rgba(148,226,213,0.1);"
            " padding: 2px 8px; border-radius: 4px; font-size: 10px; font-weight: 600; margin-right: 8px; }");
    } else if (lang == "c" || lang == "cpp") {
        m_langBadge->setStyleSheet(
            "QLabel { color: #fab387; background: rgba(250,179,135,0.1);"
            " padding: 2px 8px; border-radius: 4px; font-size: 10px; font-weight: 600; margin-right: 8px; }");
    } else if (lang == "ul") {
        m_langBadge->setStyleSheet(
            "QLabel { color: #89b4fa; background: rgba(137,180,250,0.1);"
            " padding: 2px 8px; border-radius: 4px; font-size: 10px; font-weight: 600; margin-right: 8px; }");
    } else {
        m_langBadge->setStyleSheet(
            "QLabel { color: #a6adc8; background: rgba(166,173,200,0.1);"
            " padding: 2px 8px; border-radius: 4px; font-size: 10px; font-weight: 600; margin-right: 8px; }");
    }
}

QString EditorWidget::detectLanguage(const QString& filePath) const
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    if (ext == "py")   return "python";
    if (ext == "c" || ext == "h") return "c";
    if (ext == "cpp" || ext == "cxx" || ext == "cc" || ext == "hpp") return "cpp";
    if (ext == "rs")   return "rust";
    if (ext == "ul")   return "ul";
    if (ext == "js")   return "javascript";
    return "text";
}

QString EditorWidget::languageDisplay(const QString& lang) const
{
    if (lang == "python") return "Python 3.12";
    if (lang == "c")      return "C (gcc)";
    if (lang == "cpp")    return "C++ (g++)";
    if (lang == "rust")   return "Rust (rustc)";
    if (lang == "ul")     return "UniLogic";
    return "Plain Text";
}

void EditorWidget::showWelcome()
{
    m_editorStack->setCurrentIndex(0);
}

void EditorWidget::hideWelcome()
{
    m_editorStack->setCurrentIndex(1);
}

void EditorWidget::applyCloseButtonStyle(QPushButton* btn, bool isDark) const
{
    if (isDark) {
        btn->setStyleSheet(
            "QPushButton { background: transparent; color: #6c7086;"
            " border: none; font-size: 11px; padding: 0; border-radius: 2px; }"
            "QPushButton:hover { color: #f38ba8; background: rgba(243,139,168,0.15); }");
    } else {
        btn->setStyleSheet(
            "QPushButton { background: transparent; color: #999999;"
            " border: none; font-size: 11px; padding: 0; border-radius: 2px; }"
            "QPushButton:hover { color: #cc0000; background: rgba(204,0,0,0.08); }");
    }
}

QPushButton* EditorWidget::makeCloseButton(int tabIdx)
{
    auto* btn = new QPushButton(QString::fromUtf8("\xc3\x97"));  // ×
    btn->setFixedSize(16, 16);
    btn->setCursor(Qt::PointingHandCursor);
    applyCloseButtonStyle(btn, m_isDark);
    connect(btn, &QPushButton::clicked, this, [this, btn]() {
        // Find which tab this button belongs to
        for (int i = 0; i < m_tabBar->count(); ++i) {
            if (m_tabBar->tabButton(i, QTabBar::RightSide) == btn) {
                emit m_tabBar->tabCloseRequested(i);
                return;
            }
        }
    });
    return btn;
}

void EditorWidget::applyTheme(bool isDark)
{
    m_isDark = isDark;
    m_codeEditor->applyTheme(isDark);

    if (isDark) {
        // Tab row
        auto* tabRow = m_tabBar->parentWidget();
        if (tabRow)
            tabRow->setStyleSheet("background: #2a2a3c; border-bottom: 1px solid #313244;");

        // Welcome page
        m_welcomePage->setStyleSheet("background: #1e1e2e;");

        // Welcome page labels
        auto* title = m_welcomePage->findChild<QLabel*>();
        if (title)
            title->setStyleSheet(
                "QLabel { color: #cdd6f4; font-size: 18px; font-weight: 700; background: transparent; }");

        // Code block in welcome page
        auto* codeBlock = m_welcomePage->findChild<QPlainTextEdit*>("welcomeCodeBlock");
        if (codeBlock)
            codeBlock->setStyleSheet(
                "QPlainTextEdit {"
                "  background: #2a2a3c; color: #cdd6f4; border: 1px solid #45475a;"
                "  border-radius: 6px; padding: 12px;"
                "  font-family: 'Cascadia Code', 'Consolas', monospace; font-size: 13px;"
                "}");

        // Language badge
        m_langBadge->setStyleSheet(
            "QLabel { color: #94e2d5; background: rgba(148,226,213,0.1);"
            " padding: 2px 8px; border-radius: 4px; font-size: 10px; font-weight: 600;"
            " margin-right: 4px; }");
    } else {
        // Tab row
        auto* tabRow = m_tabBar->parentWidget();
        if (tabRow)
            tabRow->setStyleSheet("background: #f0f0f0; border-bottom: 1px solid #d0d0d0;");

        // Welcome page
        m_welcomePage->setStyleSheet("background: #ffffff;");

        // Welcome page labels
        auto* title = m_welcomePage->findChild<QLabel*>();
        if (title)
            title->setStyleSheet(
                "QLabel { color: #1e1e2e; font-size: 18px; font-weight: 700; background: transparent; }");

        // Code block in welcome page
        auto* codeBlock = m_welcomePage->findChild<QPlainTextEdit*>("welcomeCodeBlock");
        if (codeBlock)
            codeBlock->setStyleSheet(
                "QPlainTextEdit {"
                "  background: #f8f8f8; color: #1e1e2e; border: 1px solid #d0d0d0;"
                "  border-radius: 6px; padding: 12px;"
                "  font-family: 'Cascadia Code', 'Consolas', monospace; font-size: 13px;"
                "}");

        // Language badge
        m_langBadge->setStyleSheet(
            "QLabel { color: #2563eb; background: rgba(37,99,235,0.1);"
            " padding: 2px 8px; border-radius: 4px; font-size: 10px; font-weight: 600;"
            " margin-right: 4px; }");
    }

    // Re-style all existing close buttons
    for (int i = 0; i < m_tabBar->count(); ++i) {
        auto* btn = qobject_cast<QPushButton*>(m_tabBar->tabButton(i, QTabBar::RightSide));
        if (btn)
            applyCloseButtonStyle(btn, isDark);
    }
}

void EditorWidget::setWelcomeCode(const QString& lang)
{
    m_welcomeLang = lang;
    auto* codeBlock = m_welcomePage->findChild<QPlainTextEdit*>("welcomeCodeBlock");
    if (!codeBlock) return;

    if (lang == "python") {
        codeBlock->setPlainText(
            "# Hello World in Python\n"
            "\n"
            "def greet(name: str) -> str:\n"
            "    return f\"Hello, {name}!\"\n"
            "\n"
            "if __name__ == \"__main__\":\n"
            "    message = greet(\"World\")\n"
            "    print(message)\n"
        );
    } else if (lang == "c") {
        codeBlock->setPlainText(
            "// Hello World in C\n"
            "\n"
            "#include <stdio.h>\n"
            "\n"
            "int main(void) {\n"
            "    printf(\"Hello, World!\\n\");\n"
            "    return 0;\n"
            "}\n"
        );
    } else if (lang == "ul") {
        codeBlock->setPlainText(
            "// Hello World in UniLogic\n"
            "\n"
            "fn main() {\n"
            "    let message = \"Hello, World!\"\n"
            "    print(message)\n"
            "}\n"
        );
    }
}
