#include "core/mainwindow.h"
#include "core/securitytesting.h"
#include "core/runtimeanalysis.h"
#include "core/theme.h"
#include "core/buildbar.h"
#include "core/buildsystem.h"
#include "core/runtimestrip.h"
#include "core/setupwizard.h"
#include "core/settingspanel.h"
#include "editor/editor.h"
#include "editor/filetree.h"
#include "panels/clarity.h"
#include "panels/aichat.h"
#include "panels/langguide.h"
#include "panels/basics.h"
#include "panels/expander.h"
#include "panels/levels.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QAction>
#include <QFileDialog>
#include <QApplication>
#include <QFileInfo>
#include <QSettings>
#include <QIcon>
#include <QTimer>
#include <QPushButton>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QScrollArea>
#include <QGridLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QTemporaryFile>
#include <QProcess>
#include <QTextCursor>
#include <QTextBlock>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Code Clarity");
    resize(1280, 800);

    // Window icon (taskbar + title bar) — load from file next to executable
    {
        QString iconPath = QApplication::applicationDirPath() + "/unilogic.ico";
        QIcon icon(iconPath);
        if (icon.isNull())
            icon = QIcon(":/unilogic.ico"); // fallback to Qt resource if present
        setWindowIcon(icon);
    }

    // Create non-visual helpers
    m_explainHandler = new ExplainHandler(this);
    m_walkHandler = new WalkThroughHandler(this);
    m_bfsMode = new BuildFromScratchMode(this);

    // Restore theme preference before creating UI so first paint uses right theme
    {
        QSettings themeS("CodeClarity", "CodeClarity");
        m_isDarkTheme = themeS.value("theme/dark", true).toBool();
        applyTheme();
    }

    createMenuBar();
    setupCentralLayout();
    createStatusBar();
    wireSignals();

    // Build system — detect toolchains now; long probes run fast on first call
    m_buildSystem = new BuildSystem(this);

    restoreSession();
    // Re-apply theme now that all widgets exist (constructors use dark defaults)
    applyTheme();

    // Show setup wizard on first launch (when no AI provider is configured)
    QSettings s("CodeClarity", "CodeClarity");
    if (s.value("ai/provider", "").toString().isEmpty()) {
        QTimer::singleShot(0, this, [this]() {
            SetupWizard *wizard = new SetupWizard(this);
            wizard->setAttribute(Qt::WA_DeleteOnClose);
            connect(wizard, &QDialog::finished, this, [this]() {
                if (m_aiChatPanel) m_aiChatPanel->reloadModelFromConfig();
            });
            wizard->show();
        });
    }
}

// ── Menu Bar ────────────────────────────────────────────────────────────
void MainWindow::createMenuBar()
{
    auto* mb = menuBar();

    // File
    auto* fileMenu = mb->addMenu("&File");
    auto* newFile = fileMenu->addAction("New File");
    newFile->setShortcut(QKeySequence("Ctrl+N"));
    connect(newFile, &QAction::triggered, this, [this]() {
        if (m_editor) m_editor->newUntitled();
    });

    auto* openFile = fileMenu->addAction("Open File...");
    openFile->setShortcut(QKeySequence("Ctrl+O"));
    connect(openFile, &QAction::triggered, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Open File", QString(),
            "All Files (*);;Python (*.py);;C (*.c *.h);;UniLogic (*.ul)");
        if (!path.isEmpty() && m_editor) {
            m_editor->openFile(path);
        }
    });

    auto* openFolder = fileMenu->addAction("Open Folder...");
    openFolder->setShortcut(QKeySequence("Ctrl+Shift+O"));
    connect(openFolder, &QAction::triggered, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Open Folder");
        if (!dir.isEmpty() && m_fileTree) {
            m_fileTree->setRootFolder(dir);
            addToRecentWorkspaces(dir);
        }
    });

    fileMenu->addSeparator();

    // Recent Workspaces submenu
    m_recentMenu = fileMenu->addMenu("Recent Workspaces");
    rebuildRecentWorkspacesMenu();

    fileMenu->addSeparator();

    auto* save = fileMenu->addAction("Save");
    save->setShortcut(QKeySequence("Ctrl+S"));
    connect(save, &QAction::triggered, this, [this]() {
        if (m_editor) m_editor->saveCurrentFile();
    });

    auto* saveAs = fileMenu->addAction("Save As...");
    saveAs->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(saveAs, &QAction::triggered, this, [this]() {
        if (m_editor) m_editor->saveCurrentFileAs();
    });

    fileMenu->addSeparator();

    auto* settingsAction = fileMenu->addAction("Settings...");
    settingsAction->setShortcut(QKeySequence("Ctrl+,"));
    connect(settingsAction, &QAction::triggered, this, [this]() {
        SettingsPanel dlg(this);
        connect(&dlg, &SettingsPanel::modelsChanged, this, [this]() {
            if (m_aiChatPanel) m_aiChatPanel->reloadModelFromConfig();
        });
        dlg.exec();
        if (m_aiChatPanel) m_aiChatPanel->reloadModelFromConfig();
    });

    fileMenu->addSeparator();

    auto* exit = fileMenu->addAction("Exit");
    connect(exit, &QAction::triggered, qApp, &QApplication::quit);

    // Edit
    auto* editMenu = mb->addMenu("&Edit");

    auto* undo = editMenu->addAction("Undo");
    undo->setShortcut(QKeySequence("Ctrl+Z"));
    connect(undo, &QAction::triggered, this, [this]() {
        if (m_editor) m_editor->undo();
    });

    auto* redo = editMenu->addAction("Redo");
    redo->setShortcut(QKeySequence("Ctrl+Y"));
    connect(redo, &QAction::triggered, this, [this]() {
        if (m_editor) m_editor->redo();
    });

    editMenu->addSeparator();

    auto* find = editMenu->addAction("Find");
    find->setShortcut(QKeySequence("Ctrl+F"));
    connect(find, &QAction::triggered, this, [this]() {
        if (!m_editor) return;
        auto *dlg = new QDialog(this);
        dlg->setWindowTitle("Find");
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setMinimumWidth(340);
        auto *layout = new QVBoxLayout(dlg);
        layout->setSpacing(8);
        auto *row = new QHBoxLayout;
        auto *lbl = new QLabel("Find:", dlg);
        auto *input = new QLineEdit(dlg);
        input->setPlaceholderText("Search text...");
        row->addWidget(lbl);
        row->addWidget(input, 1);
        layout->addLayout(row);
        auto *btnRow = new QHBoxLayout;
        auto *prevBtn = new QPushButton("Previous", dlg);
        auto *nextBtn = new QPushButton("Next", dlg);
        auto *closeBtn = new QPushButton("Close", dlg);
        btnRow->addWidget(prevBtn);
        btnRow->addWidget(nextBtn);
        btnRow->addStretch();
        btnRow->addWidget(closeBtn);
        layout->addLayout(btnRow);
        connect(nextBtn, &QPushButton::clicked, dlg, [this, input]() {
            if (!m_editor) return;
            m_editor->codeEditor()->find(input->text());
        });
        connect(prevBtn, &QPushButton::clicked, dlg, [this, input]() {
            if (!m_editor) return;
            m_editor->codeEditor()->find(input->text(), QTextDocument::FindBackward);
        });
        connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
        connect(input, &QLineEdit::returnPressed, nextBtn, &QPushButton::click);
        dlg->show();
        input->setFocus();
    });

    auto* replace = editMenu->addAction("Replace");
    replace->setShortcut(QKeySequence("Ctrl+H"));
    connect(replace, &QAction::triggered, this, [this]() {
        if (!m_editor) return;
        auto *dlg = new QDialog(this);
        dlg->setWindowTitle("Find & Replace");
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setMinimumWidth(360);
        auto *layout = new QVBoxLayout(dlg);
        layout->setSpacing(8);
        auto *grid = new QGridLayout;
        grid->addWidget(new QLabel("Find:", dlg),    0, 0);
        auto *findInput = new QLineEdit(dlg);
        findInput->setPlaceholderText("Search text...");
        grid->addWidget(findInput, 0, 1);
        grid->addWidget(new QLabel("Replace:", dlg), 1, 0);
        auto *replInput = new QLineEdit(dlg);
        replInput->setPlaceholderText("Replacement text...");
        grid->addWidget(replInput, 1, 1);
        layout->addLayout(grid);
        auto *btnRow = new QHBoxLayout;
        auto *replNext = new QPushButton("Replace Next", dlg);
        auto *replAll  = new QPushButton("Replace All", dlg);
        auto *closeBtn = new QPushButton("Close", dlg);
        btnRow->addWidget(replNext);
        btnRow->addWidget(replAll);
        btnRow->addStretch();
        btnRow->addWidget(closeBtn);
        layout->addLayout(btnRow);
        connect(replNext, &QPushButton::clicked, dlg, [this, findInput, replInput]() {
            if (!m_editor) return;
            auto *ed = m_editor->codeEditor();
            if (ed->find(findInput->text())) {
                QTextCursor cur = ed->textCursor();
                cur.insertText(replInput->text());
            }
        });
        connect(replAll, &QPushButton::clicked, dlg, [this, findInput, replInput]() {
            if (!m_editor) return;
            auto *ed = m_editor->codeEditor();
            QString text = ed->toPlainText();
            text.replace(findInput->text(), replInput->text());
            ed->setPlainText(text);
        });
        connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
        dlg->show();
        findInput->setFocus();
    });

    // Tools
    auto* toolsMenu = mb->addMenu("&Tools");

    auto* langGuideAction = toolsMenu->addAction("Language Guide");
    connect(langGuideAction, &QAction::triggered, this, [this]() {
        if (m_langGuide && m_editor) {
            m_langGuide->showForLanguage(m_editor->currentLanguage());
        }
    });

    auto* expanderAction = toolsMenu->addAction("Statement Expander");
    connect(expanderAction, &QAction::triggered, this, [this]() {
        if (m_expanderOverlay) {
            m_expanderOverlay->showDefaultDemo();
        }
    });

    auto* basicsAction = toolsMenu->addAction("Tutorials && Examples");
    connect(basicsAction, &QAction::triggered, this, [this]() {
        if (m_basicsOverlay) {
            m_basicsOverlay->goToPage(0);
            m_basicsOverlay->setVisible(true);
            m_basicsOverlay->raise();
        }
    });

    auto* explainAction = toolsMenu->addAction("Explain Codebase");
    connect(explainAction, &QAction::triggered, this, [this]() {
        if (!m_editor || !m_explainHandler) return;
        QString filePath = m_editor->currentFilePath();
        if (filePath.isEmpty()) return;
        QFileInfo fi(filePath);
        QString filename = fi.fileName();
        if (m_explainHandler->hasExplanation(filename)) {
            QString explained = m_explainHandler->getExplainedCode(filename);
            m_editor->codeEditor()->setPlainText(explained);
        }
    });

    toolsMenu->addSeparator();

    auto* runtimeInfo = toolsMenu->addAction("Runtime Info");
    runtimeInfo->setShortcut(QKeySequence("Ctrl+R"));
    connect(runtimeInfo, &QAction::triggered, this, [this]() {
        if (m_runtimeStrip) m_runtimeStrip->toggle();
    });

    toolsMenu->addSeparator();

    auto* securityAction = toolsMenu->addAction("Security Testing");
    securityAction->setShortcut(QKeySequence("Ctrl+Shift+T"));
    connect(securityAction, &QAction::triggered, this, [this]() {
        if (!m_securityFrame) return;
        // Pass current code, language, and file path to the frame
        QString code;
        QString lang;
        QString filePath;
        if (m_editor) {
            code     = m_editor->currentContent();
            lang     = currentLangKey();
            filePath = m_editor->currentFilePath();
        }
        m_securityFrame->setCode(code, lang, filePath);

        // Switch view: hide editor splitter, show security frame
        m_mainSplitter->setVisible(false);
        m_securityFrame->setVisible(true);
    });

    auto* runtimeAnalysisAction = toolsMenu->addAction("Runtime Analysis...");
    runtimeAnalysisAction->setShortcut(QKeySequence("Ctrl+Shift+R"));
    connect(runtimeAnalysisAction, &QAction::triggered, this, [this]() {
        if (!m_runtimeAnalysis) return;
        // Hide editor layout, show runtime analysis frame
        m_mainSplitter->setVisible(false);
        if (m_securityFrame) m_securityFrame->setVisible(false);
        m_runtimeAnalysis->setVisible(true);
    });

    // Help
    auto* helpMenu = mb->addMenu("&Help");

    auto* gettingStarted = helpMenu->addAction("Getting Started");
    connect(gettingStarted, &QAction::triggered, this, [this]() {
        QMessageBox::information(this, "Getting Started",
            "1. Set up your AI model (File > Settings > Models)\n"
            "2. Open a folder or file\n"
            "3. Ask the AI to build something in the chat\n"
            "4. Read the Clarity panel to learn what happened\n"
            "5. Use Explain or Walk Me Through for deeper understanding");
    });

    auto* keyboardShortcuts = helpMenu->addAction("Keyboard Shortcuts");
    connect(keyboardShortcuts, &QAction::triggered, this, [this]() {
        QMessageBox::information(this, "Keyboard Shortcuts",
            "Ctrl+N    New File\n"
            "Ctrl+O    Open File\n"
            "Ctrl+Shift+O    Open Folder\n"
            "Ctrl+S    Save\n"
            "Ctrl+Shift+S    Save As\n"
            "Ctrl+,    Settings\n"
            "Ctrl+F    Find\n"
            "Ctrl+H    Replace");
    });

    auto* aboutAction = helpMenu->addAction("About Code Clarity");
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "About Code Clarity",
            "Code Clarity v0.1.0\n\n"
            "A code editor and learning platform for those new to programming or anyone looking to tie explicit learning to their coding work.\n\n"
            "https://github.com/albazzaztariq/UL-Code-Clarity\n\n"
            "Built with Qt 6.8.3\n"
            "© 2026 UniLogic Project");
    });

    helpMenu->addSeparator();

    auto* reportIssue = helpMenu->addAction("Report an Issue...");
    connect(reportIssue, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(
            QUrl("https://github.com/albazzaztariq/UniLogic/issues/new?labels=code-clarity&template=bug_report.md&title=[Code+Clarity]+"));
    });

}

// ── Status Bar ──────────────────────────────────────────────────────────
void MainWindow::createStatusBar()
{
    auto* sb = statusBar();

    m_statusReady = new QLabel("Ready");
    m_statusReady->setStyleSheet(
        "QLabel { color: #a6adc8; font-size: 11px; padding: 0 6px; background: transparent; }");
    sb->addWidget(m_statusReady);

    // Green dot indicator
    auto* dot = new QLabel;
    dot->setFixedSize(6, 6);
    dot->setStyleSheet(
        "QLabel { background: #a6e3a1; border-radius: 3px; }");
    sb->addWidget(dot);

    // Language label — hidden until a file is opened
    m_statusLang = new QLabel;
    m_statusLang->setStyleSheet("QLabel { color: #a6adc8; font-size: 11px; padding: 0 8px; background: transparent; }");
    m_statusLang->hide();
    sb->addWidget(m_statusLang);

    m_statusEnc = new QLabel("UTF-8");
    m_statusEnc->setStyleSheet("QLabel { color: #a6adc8; font-size: 11px; padding: 0 8px; background: transparent; }");
    sb->addWidget(m_statusEnc);

    m_statusPos = new QLabel("Ln 1, Col 1");
    m_statusPos->setStyleSheet("QLabel { color: #a6adc8; font-size: 11px; padding: 0 8px; background: transparent; }");
    sb->addWidget(m_statusPos);

    // Spacer — background must match status bar, updated in applyTheme
    m_statusBarSpacer = new QWidget;
    m_statusBarSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_statusBarSpacer->setStyleSheet("background: #181825;");
    sb->addWidget(m_statusBarSpacer);

    // Level selector in status bar (segmented button group)
    m_levelSelector = new LevelSelector;
    sb->addPermanentWidget(m_levelSelector);

    // Gear icon — opens Settings dialog
    m_settingsGearBtn = new QPushButton(QString::fromUtf8("\xe2\x9a\x99"), this); // ⚙
    auto* settingsGearBtn = m_settingsGearBtn;
    settingsGearBtn->setFixedSize(22, 22);
    settingsGearBtn->setCursor(Qt::PointingHandCursor);
    settingsGearBtn->setToolTip("Settings");
    settingsGearBtn->setStyleSheet(
        "QPushButton { background: none; color: #a6adc8; font-size: 13px; border: none; padding: 0; }"
        "QPushButton:hover { color: #cdd6f4; }");
    connect(settingsGearBtn, &QPushButton::clicked, this, [this]() {
        SettingsPanel dlg(this);
        connect(&dlg, &SettingsPanel::modelsChanged, this, [this]() {
            if (m_aiChatPanel) m_aiChatPanel->reloadModelFromConfig();
        });
        dlg.exec();
        if (m_aiChatPanel) m_aiChatPanel->reloadModelFromConfig();
    });
    sb->addPermanentWidget(settingsGearBtn);

    // Theme toggle button (moon = dark, sun = light)
    QSettings themeSettings("CodeClarity", "CodeClarity");
    m_isDarkTheme = themeSettings.value("theme/dark", true).toBool();

    m_themeToggleBtn = new QPushButton(this);
    m_themeToggleBtn->setFixedSize(22, 22);
    m_themeToggleBtn->setCursor(Qt::PointingHandCursor);
    m_themeToggleBtn->setToolTip("Toggle Light/Dark Theme");
    m_themeToggleBtn->setStyleSheet(
        "QPushButton { background: none; color: #a6adc8; font-size: 13px; border: none; padding: 0; }"
        "QPushButton:hover { color: #cdd6f4; }");
    auto updateThemeIcon = [this]() {
        // Moon = currently dark (click to go light), Sun = currently light (click to go dark)
        m_themeToggleBtn->setText(m_isDarkTheme
            ? QString::fromUtf8("\xf0\x9f\x8c\x99")   // 🌙
            : QString::fromUtf8("\xe2\x98\x80"));       // ☀
    };
    updateThemeIcon();
    connect(m_themeToggleBtn, &QPushButton::clicked, this, [this, updateThemeIcon]() {
        m_isDarkTheme = !m_isDarkTheme;
        QSettings s("CodeClarity", "CodeClarity");
        s.setValue("theme/dark", m_isDarkTheme);
        updateThemeIcon();
        applyTheme();
    });
    sb->addPermanentWidget(m_themeToggleBtn);

    // Connect editor cursor position to status bar
    connect(m_editor, &EditorWidget::cursorPositionUpdated, this,
        [this](int line, int col) {
            m_statusPos->setText(QString("Ln %1, Col %2").arg(line).arg(col));
        });

    connect(m_editor, &EditorWidget::languageChanged, this,
        [this](const QString& lang) {
            if (lang.isEmpty()) {
                m_statusLang->hide();
                if (m_runtimeStrip) m_runtimeStrip->setVisible(false);
            } else {
                m_statusLang->setText(lang);
                m_statusLang->show();
                if (m_runtimeStrip) {
                    m_runtimeStrip->setLanguage(lang);
                    m_runtimeStrip->setVisible(true);
                }
            }
        });
}

// ── Central Layout ──────────────────────────────────────────────────────
void MainWindow::setupCentralLayout()
{
    auto* centralWidget = new QWidget;
    auto* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Runtime strip — hidden until a file is opened
    m_runtimeStrip = new RuntimeStrip;
    m_runtimeStrip->setVisible(false);
    mainLayout->addWidget(m_runtimeStrip);

    // Main horizontal splitter: file tree | center editor | right panel
    m_mainSplitter = new QSplitter(Qt::Horizontal);
    m_mainSplitter->setHandleWidth(1);
    m_mainSplitter->setChildrenCollapsible(true);

    // Left: File tree
    m_fileTree = new FileTreeWidget;
    m_fileTree->setMinimumWidth(140);
    m_fileTree->setMaximumWidth(400);
    m_mainSplitter->addWidget(m_fileTree);

    // Center: Editor (with results pane below)
    m_editor = new EditorWidget;
    m_mainSplitter->addWidget(m_editor);

    // Right: Two side-by-side columns (Clarity + AI Chat)
    m_rightSplitter = new QSplitter(Qt::Horizontal);
    m_rightSplitter->setHandleWidth(1);
    m_rightSplitter->setChildrenCollapsible(true);

    // Clarity column (left side of right panel)
    m_clarityColumn = new QWidget;
    m_clarityColumn->setStyleSheet(
        "background: #2a2a3c; border-left: 1px solid #313244;"
        " border-right: 1px solid #313244;");
    auto* clarityLayout = new QVBoxLayout(m_clarityColumn);
    clarityLayout->setContentsMargins(0, 0, 0, 0);
    clarityLayout->setSpacing(0);

    // Add ClarityPanel into the clarity column
    m_clarityPanel = new ClarityPanel;
    clarityLayout->addWidget(m_clarityPanel);

    m_rightSplitter->addWidget(m_clarityColumn);

    // AI Chat column (right side of right panel)
    m_chatColumn = new QWidget;
    m_chatColumn->setStyleSheet("background: #2a2a3c;");
    auto* chatLayout = new QVBoxLayout(m_chatColumn);
    chatLayout->setContentsMargins(0, 0, 0, 0);
    chatLayout->setSpacing(0);

    // Add AIChatPanel into the chat column
    m_aiChatPanel = new AIChatPanel;
    chatLayout->addWidget(m_aiChatPanel);

    m_rightSplitter->addWidget(m_chatColumn);

    // Each column ~200px, total ~400px
    m_rightSplitter->setSizes({200, 200});
    m_rightSplitter->setStretchFactor(0, 1);
    m_rightSplitter->setStretchFactor(1, 1);

    m_mainSplitter->addWidget(m_rightSplitter);

    // Set splitter sizes: 200 | stretch | 400
    m_mainSplitter->setSizes({200, 680, 400});
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setStretchFactor(2, 0);

    mainLayout->addWidget(m_mainSplitter, 1);

    // Security Testing Frame — sits in the same layout slot, hidden by default
    m_securityFrame = new SecurityTestingFrame;
    m_securityFrame->setVisible(false);
    mainLayout->addWidget(m_securityFrame);

    // Runtime Analysis Frame — sits in the same layout slot, hidden by default
    m_runtimeAnalysis = new RuntimeAnalysisFrame;
    m_runtimeAnalysis->setVisible(false);
    mainLayout->addWidget(m_runtimeAnalysis);

    // Build bar at bottom
    m_buildBar = new BuildBar;
    mainLayout->addWidget(m_buildBar);

    setCentralWidget(centralWidget);

    // Overlays (parented to central widget so they float above)
    m_langGuide = new LangGuideOverlay(centralWidget);
    m_basicsOverlay = new BasicsOverlay(centralWidget);
    m_expanderOverlay = new ExpanderOverlay(centralWidget);

    // Connect file tree to editor
    connect(m_fileTree, &FileTreeWidget::fileSelected, m_editor, &EditorWidget::openFile);

    // Connect "Add Workspace" button in file tree
    connect(m_fileTree, &FileTreeWidget::addWorkspaceRequested, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Add Workspace Folder");
        if (!dir.isEmpty() && m_fileTree) {
            m_fileTree->addWorkspaceFolder(dir);
            addToRecentWorkspaces(dir);
            m_aiPermissions.setWorkspaceRoot(dir);
        }
    });

    // Connect welcome page "Open File" / "Open Folder" buttons
    connect(m_editor, &EditorWidget::openFileRequested, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Open File", QString(),
            "All Files (*);;Python (*.py);;C (*.c *.h);;UniLogic (*.ul)");
        if (!path.isEmpty() && m_editor)
            m_editor->openFile(path);
    });
    connect(m_editor, &EditorWidget::openFolderRequested, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Open Folder");
        if (!dir.isEmpty() && m_fileTree) {
            m_fileTree->setRootFolder(dir);
            addToRecentWorkspaces(dir);
            m_aiPermissions.setWorkspaceRoot(dir);
        }
    });

    // When a real file is opened, clear example content from Clarity and AI Chat
    connect(m_editor, &EditorWidget::fileOpened, this, [this](const QString&) {
        if (m_aiChatPanel) m_aiChatPanel->markExamplesSeen();
        if (m_clarityPanel) m_clarityPanel->markExamplesSeen();
    });

    // ── Build system wiring ──────────────────────────────────────────────

    // BuildSystem → output pane: append HTML chunks
    connect(m_buildSystem, &BuildSystem::outputReady, this,
        [this](const QString& html) {
            // Append to results pane (raw HTML — BuildBar displays as rich text)
            QTextEdit* te = m_buildBar->resultsContent();
            if (te) {
                te->moveCursor(QTextCursor::End);
                te->insertHtml(html);
            }
        });

    // BuildSystem → finished
    connect(m_buildSystem, &BuildSystem::finished, this,
        [this](bool success) {
            Q_UNUSED(success);
            // Re-enable run/build buttons (could gray them during run)
        });

    // BuildSystem → missing python deps → prompt to install
    connect(m_buildSystem, &BuildSystem::missingDepsDetected, this,
        [this](const QStringList& pkgs, const QString& installCmd) {
            int level = m_levelSelector ? m_levelSelector->currentLevel() : 1;
            QString body;
            if (level <= 2) {
                body = QString(
                    "This file needs the following packages which aren't installed:\n\n"
                    "  %1\n\n"
                    "Do you want to install them now?"
                ).arg(pkgs.join(", "));
            } else {
                body = QString("Missing packages: %1\n\nRun: %2\n\nInstall now?")
                       .arg(pkgs.join(", "), installCmd);
            }
            auto reply = QMessageBox::question(this, "Missing Packages", body,
                             QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                // Install via pip then re-run
                QString filePath = m_editor ? m_editor->currentFilePath() : QString();
                QString lang = currentLangKey();
                m_buildBar->clearResults();

                // Launch pip install
                QProcess* pip = new QProcess(this);
                pip->setProcessChannelMode(QProcess::MergedChannels);
                QStringList pipArgs = {"-m", "pip", "install"};
                pipArgs << pkgs;
                connect(pip, &QProcess::readyRead, this, [this, pip]() {
                    QTextEdit* te = m_buildBar->resultsContent();
                    if (te) {
                        te->moveCursor(QTextCursor::End);
                        te->insertPlainText(QString::fromUtf8(pip->readAll()));
                    }
                });
                connect(pip, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, pip, filePath, lang]
                    (int exitCode, QProcess::ExitStatus) {
                        pip->deleteLater();
                        if (exitCode == 0 && !filePath.isEmpty()) {
                            int lvl = m_levelSelector ? m_levelSelector->currentLevel() : 1;
                            m_buildSystem->runFile(filePath, lang, lvl);
                        }
                    });
                m_buildBar->showResults();
                pip->start("python", pipArgs);
                if (!pip->waitForStarted(3000))
                    pip->start("py", pipArgs);
            }
        });

    // BuildSystem → toolchain missing → show install link
    connect(m_buildSystem, &BuildSystem::toolchainMissing, this,
        [this](const QString& lang, const QString& url) {
            Q_UNUSED(lang);
            int level = m_levelSelector ? m_levelSelector->currentLevel() : 1;
            QString title = "Toolchain Not Found";
            QString body;
            if (level <= 2) {
                body = QString(
                    "To run this type of file, you need to install additional software.\n\n"
                    "Would you like to open the download page?"
                );
            } else {
                body = QString("Required toolchain not found.\nInstall URL: %1\n\nOpen?").arg(url);
            }
            if (!url.isEmpty()) {
                auto reply = QMessageBox::question(this, title, body,
                                 QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes)
                    QDesktopServices::openUrl(QUrl(url));
            } else {
                QMessageBox::warning(this, title, body);
            }
        });

    // Run button — save file first, then run
    connect(m_buildBar, &BuildBar::runRequested, this, [this]() {
        if (!m_editor) return;
        QString filePath = m_editor->currentFilePath();
        if (filePath.isEmpty()) {
            // Unsaved buffer — run from temp file
            QTemporaryFile* tmp = new QTemporaryFile(this);
            QString ext;
            QString lk = currentLangKey();
            if (lk == "python")     ext = ".py";
            else if (lk == "c")     ext = ".c";
            else if (lk == "cpp")   ext = ".cpp";
            else if (lk == "rust")  ext = ".rs";
            else if (lk == "javascript") ext = ".js";
            tmp->setFileTemplate(QDir::tempPath() + "/ccrun_XXXXXX" + ext);
            if (tmp->open()) {
                tmp->write(m_editor->currentContent().toUtf8());
                tmp->flush();
                filePath = tmp->fileName();
                tmp->setAutoRemove(true);
            }
        } else {
            m_editor->saveCurrentFile();
        }
        m_buildBar->clearResults();
        m_buildBar->showResults();
        int level = m_levelSelector ? m_levelSelector->currentLevel() : 1;
        m_buildSystem->runFile(filePath, currentLangKey(), level);
    });

    // Build button — compile only
    connect(m_buildBar, &BuildBar::buildRequested, this, [this]() {
        if (!m_editor) return;
        QString filePath = m_editor->currentFilePath();
        if (filePath.isEmpty()) {
            QMessageBox::information(this, "Save First",
                "Please save your file before building.");
            return;
        }
        m_editor->saveCurrentFile();
        m_buildBar->clearResults();
        m_buildBar->showResults();
        int level = m_levelSelector ? m_levelSelector->currentLevel() : 1;
        m_buildSystem->buildFile(filePath, currentLangKey(), level);
    });
}

// ── Wire All Signals ────────────────────────────────────────────────────
void MainWindow::wireSignals()
{
    // Level selector -> clarity panel verbosity + AI assist level
    connect(m_levelSelector, &LevelSelector::levelChanged, this, [this](int level) {
        m_clarityPanel->setLevel(level);
        if (m_aiChatPanel) m_aiChatPanel->setAssistLevel(level);
        if (m_runtimeAnalysis) m_runtimeAnalysis->setAssistLevel(level);
    });
    // Sync level on open
    if (m_runtimeAnalysis && m_levelSelector)
        m_runtimeAnalysis->setAssistLevel(m_levelSelector->currentLevel());

    // Level selector -> progressive disclosure on build bar
    connect(m_levelSelector, &LevelSelector::showBuildButton, this, [this](bool show) {
        Q_UNUSED(show);
        // BuildBar build button visibility could be toggled here
    });

    // Level selector -> file tree visibility
    connect(m_levelSelector, &LevelSelector::showFileTree, this, [this](bool show) {
        m_fileTree->setVisible(show);
    });

    // Level selector -> basics button visibility
    connect(m_levelSelector, &LevelSelector::showBasicsButton, this, [this](bool show) {
        Q_UNUSED(show);
        // Basics availability could be toggled here
    });

    // Clarity panel close -> hide clarity column
    connect(m_clarityPanel, &ClarityPanel::closeRequested, this, [this]() {
        m_clarityColumn->setVisible(false);
    });

    // AI Chat panel close -> hide chat column
    connect(m_aiChatPanel, &AIChatPanel::closeRequested, this, [this]() {
        m_chatColumn->setVisible(false);
    });

    // Overlay close signals
    connect(m_langGuide, &LangGuideOverlay::closeRequested, m_langGuide, &QWidget::hide);
    connect(m_basicsOverlay, &BasicsOverlay::closeRequested, m_basicsOverlay, &QWidget::hide);
    connect(m_expanderOverlay, &ExpanderOverlay::closeRequested, m_expanderOverlay, &QWidget::hide);

    // Build from Scratch mode
    connect(m_bfsMode, &BuildFromScratchMode::activated, this, [this]() {
        m_aiChatPanel->setBuildFromScratchWelcome();
    });
    connect(m_bfsMode, &BuildFromScratchMode::deactivated, this, [this]() {
        m_aiChatPanel->setStandardMode();
    });
    connect(m_bfsMode, &BuildFromScratchMode::scaffoldCode, this, [this](const QString& code) {
        if (m_editor) {
            m_editor->codeEditor()->setPlainText(code);
        }
    });

    // AI Chat message -> route to BFS or real AI backend
    connect(m_aiChatPanel, &AIChatPanel::messageSent, this, [this](const QString& text) {
        if (m_bfsMode->isActive()) {
            QString response = m_bfsMode->generateResponse(text);
            m_aiChatPanel->addMessage(ChatBubble::AI, response);
            m_aiChatPanel->setInputEnabled(true);
        } else {
            // Real AI call — streaming response arrives via responseChunk → onResponseChunk
            m_aiChatPanel->sendAIMessage(text);
        }
    });

    // Walk through handler
    connect(m_walkHandler, &WalkThroughHandler::walkthroughGenerated, this,
        [this](const QString& explanation) {
            m_aiChatPanel->addMessage(ChatBubble::AI, explanation);
        });

    // Editor toolbar: Explain button — annotates current code locally, no model needed
    connect(m_editor, &EditorWidget::explainRequested, this, [this]() {
        if (!m_editor) return;
        QString code = m_editor->codeEditor()->toPlainText();
        if (code.trimmed().isEmpty()) return;

        // Save the original before we overwrite it
        m_preExplainCode = code;
        m_explainActive = true;

        QString lang  = m_editor->currentLanguage();  // e.g. "Python 3.12"
        // Normalise to internal lang key
        QString langKey;
        if (lang.startsWith("Python"))      langKey = "python";
        else if (lang.startsWith("C++"))    langKey = "cpp";
        else if (lang.startsWith("C "))     langKey = "c";
        else if (lang.startsWith("Rust"))   langKey = "rust";
        else if (lang.startsWith("UniLogic")) langKey = "ul";
        else                                langKey = "text";

        int level = m_levelSelector ? m_levelSelector->currentLevel() : 1;
        QString explained = ExplainHandler::generateExplainedCode(code, langKey, level);
        m_editor->codeEditor()->setPlainText(explained);
        m_editor->setExplainActive(true);
    });

    // Editor toolbar: Clear Explanations button — restores the pre-explain original
    connect(m_editor, &EditorWidget::clearExplainRequested, this, [this]() {
        if (!m_editor || !m_explainActive) return;
        m_editor->codeEditor()->setPlainText(m_preExplainCode);
        m_preExplainCode.clear();
        m_explainActive = false;
        m_editor->setExplainActive(false);
    });

    // Editor toolbar: Walk Me Through button
    connect(m_editor, &EditorWidget::walkThroughRequested, this, [this]() {
        if (!m_editor || !m_walkHandler) return;
        QString filePath = m_editor->currentFilePath();
        QFileInfo fi(filePath);
        // Use the filename if available; fall back to "untitled" for unsaved buffers
        QString filename = fi.fileName().isEmpty() ? "untitled" : fi.fileName();
        // Use currentContent() which reads directly from the active tab's editor widget
        QString code = m_editor->currentContent();
        if (code.trimmed().isEmpty()) {
            if (m_aiChatPanel)
                m_aiChatPanel->addMessage(ChatBubble::AI,
                    "No code to walk through. Please open a file first.");
            return;
        }
        int level = m_levelSelector ? m_levelSelector->currentLevel() : 1;
        QString walkthrough = m_walkHandler->generateWalkthrough(filename, code, level);
        if (m_aiChatPanel)
            m_aiChatPanel->addMessage(ChatBubble::AI, walkthrough);
    });

    // Runtime Analysis Frame signals
    connect(m_runtimeAnalysis, &RuntimeAnalysisFrame::backToEditor, this, [this]() {
        m_runtimeAnalysis->setVisible(false);
        m_mainSplitter->setVisible(true);
    });
    connect(m_runtimeAnalysis, &RuntimeAnalysisFrame::compilationError, this,
        [this](const QString& filePath, const QString& errorText) {
            // Switch back to editor and show error in build bar
            m_runtimeAnalysis->setVisible(false);
            m_mainSplitter->setVisible(true);
            if (m_buildBar) {
                m_buildBar->clearResults();
                m_buildBar->showResults();
                QTextEdit* te = m_buildBar->resultsContent();
                if (te) {
                    te->setHtml(QString(
                        "<span style='color:#f38ba8;font-weight:bold;'>Compilation failed</span>"
                        " for <b>%1</b><br><pre style='color:#cdd6f4;'>%2</pre>")
                        .arg(QFileInfo(filePath).fileName(), errorText.toHtmlEscaped()));
                }
            }
        });

    // Security Testing Frame signals
    connect(m_securityFrame, &SecurityTestingFrame::backToEditor, this, [this]() {
        m_securityFrame->setVisible(false);
        m_mainSplitter->setVisible(true);
    });
    connect(m_securityFrame, &SecurityTestingFrame::jumpToLine, this, [this](int lineNumber) {
        if (!m_editor) return;
        // Switch back to editor so the user can see the line
        m_securityFrame->setVisible(false);
        m_mainSplitter->setVisible(true);
        // Move cursor to the requested line
        if (lineNumber > 0) {
            auto* ed = m_editor->codeEditor();
            QTextCursor cursor = ed->textCursor();
            cursor.movePosition(QTextCursor::Start);
            cursor.movePosition(QTextCursor::NextBlock,
                                QTextCursor::MoveAnchor,
                                lineNumber - 1);
            ed->setTextCursor(cursor);
            ed->centerCursor();
        }
    });

    // AI Chat: keep editor context in sync so the AI knows the current file
    connect(m_editor, &EditorWidget::fileOpened, this, [this](const QString& filePath) {
        Q_UNUSED(filePath);
        if (m_aiChatPanel && m_editor) {
            m_aiChatPanel->setEditorContext(
                m_editor->codeEditor()->toPlainText(),
                m_editor->currentLanguage()
            );
        }
    });
    connect(m_editor, &EditorWidget::languageChanged, this, [this](const QString& lang) {
        if (m_aiChatPanel && m_editor) {
            m_aiChatPanel->setEditorContext(
                m_editor->codeEditor()->toPlainText(),
                lang
            );
        }
    });

    // AI Chat: when AI returns a code block, apply it to the editor and log to Clarity
    connect(m_aiChatPanel, &AIChatPanel::codeBlockReceived, this,
        [this](const QString& code, const QString& lang) {
            if (m_editor) {
                // Permission check: AI wants to write the current file
                QString filePath = m_editor->currentFilePath();
                QString wsRoot   = m_fileTree ? m_fileTree->rootPath() : QString();

                // Only check if a file is actually open (in-memory editing needs no check)
                if (!filePath.isEmpty()) {
                    if (!wsRoot.isEmpty())
                        m_aiPermissions.setWorkspaceRoot(wsRoot);
                    if (!m_aiPermissions.requestAccess(filePath, AIPermissions::Write, this)) {
                        if (!AIPermissions::isUnderWorkspace(filePath, wsRoot)) {
                            m_aiChatPanel->addMessage(ChatBubble::AI,
                                "<b>Blocked:</b> The file is outside your workspace. "
                                "Code Clarity only applies AI edits to files within your workspace.");
                        }
                        return;
                    }
                }

                m_editor->codeEditor()->setPlainText(code);
            }
            // Add a Clarity changelog entry for the AI edit
            if (m_clarityPanel) {
                QString langLabel = lang.isEmpty() ? "code" : lang;
                int lineCount = code.count('\n') + 1;
                QString timeStr = QDateTime::currentDateTime().toString("h:mm ap");
                ClarityEntryData entry;
                entry.title = QString("AI rewrote %1 file").arg(langLabel);
                entry.time  = timeStr;
                entry.detailHtml = QString(
                    "<b>What Changed:</b> The AI assistant replaced the editor content "
                    "with a new %1 code block (%2 lines)."
                    "<br><b>Why:</b> You asked the AI to edit or generate code."
                    "<br><b>How to undo:</b> Use Edit &rarr; Undo (Ctrl+Z) in the editor."
                ).arg(langLabel).arg(lineCount);
                m_clarityPanel->addEntry(entry);
            }
        });
}

// ── Language key ─────────────────────────────────────────────────────────
QString MainWindow::currentLangKey() const
{
    QString lang = m_editor ? m_editor->currentLanguage() : QString();
    if (lang.startsWith("Python"))      return "python";
    if (lang.startsWith("C++"))         return "cpp";
    if (lang.startsWith("C ") || lang == "C") return "c";
    if (lang.startsWith("Rust"))        return "rust";
    if (lang.startsWith("JavaScript"))  return "javascript";
    if (lang.startsWith("UniLogic"))    return "ul";
    return "python";
}

// ── Theme ────────────────────────────────────────────────────────────────
void MainWindow::applyTheme()
{
    qApp->setStyleSheet(m_isDarkTheme ? Theme::appStyleSheet() : Theme::lightStyleSheet());

    // Re-apply per-widget stylesheets that hardcode colors and override the global sheet
    if (m_fileTree)
        m_fileTree->applyTheme(m_isDarkTheme);
    if (m_editor)
        m_editor->applyTheme(m_isDarkTheme);

    // Panels that use inline background styles
    if (m_clarityColumn) {
        m_clarityColumn->setStyleSheet(m_isDarkTheme
            ? "background: #2a2a3c; border-left: 1px solid #313244;"
              " border-right: 1px solid #313244;"
            : "background: #fafafa; border-left: 1px solid #e0e0e0;"
              " border-right: 1px solid #e0e0e0;");
    }
    if (m_chatColumn) {
        m_chatColumn->setStyleSheet(m_isDarkTheme
            ? "background: #2a2a3c;"
            : "background: #fafafa;");
    }

    // Status bar spacer — must match bar background to avoid white blob
    if (m_statusBarSpacer)
        m_statusBarSpacer->setStyleSheet(m_isDarkTheme
            ? "background: #181825;"
            : "background: #f0f0f0;");

    // Status bar labels — always transparent background so they blend with QStatusBar
    if (m_statusReady)
        m_statusReady->setStyleSheet(m_isDarkTheme
            ? "QLabel { color: #a6adc8; font-size: 11px; padding: 0 6px; background: transparent; }"
            : "QLabel { color: #555555; font-size: 11px; padding: 0 6px; background: transparent; }");
    if (m_statusLang)
        m_statusLang->setStyleSheet(m_isDarkTheme
            ? "QLabel { color: #a6adc8; font-size: 11px; padding: 0 8px; background: transparent; }"
            : "QLabel { color: #555555; font-size: 11px; padding: 0 8px; background: transparent; }");
    if (m_statusEnc)
        m_statusEnc->setStyleSheet(m_isDarkTheme
            ? "QLabel { color: #a6adc8; font-size: 11px; padding: 0 8px; background: transparent; }"
            : "QLabel { color: #555555; font-size: 11px; padding: 0 8px; background: transparent; }");
    if (m_statusPos)
        m_statusPos->setStyleSheet(m_isDarkTheme
            ? "QLabel { color: #a6adc8; font-size: 11px; padding: 0 8px; background: transparent; }"
            : "QLabel { color: #555555; font-size: 11px; padding: 0 8px; background: transparent; }");

    // Theme toggle and gear buttons in status bar
    if (m_themeToggleBtn)
        m_themeToggleBtn->setStyleSheet(m_isDarkTheme
            ? "QPushButton { background: none; color: #a6adc8; font-size: 13px; border: none; padding: 0; }"
              "QPushButton:hover { color: #cdd6f4; }"
            : "QPushButton { background: none; color: #555555; font-size: 13px; border: none; padding: 0; }"
              "QPushButton:hover { color: #1e1e2e; }");
    if (m_settingsGearBtn)
        m_settingsGearBtn->setStyleSheet(m_isDarkTheme
            ? "QPushButton { background: none; color: #a6adc8; font-size: 13px; border: none; padding: 0; }"
              "QPushButton:hover { color: #cdd6f4; }"
            : "QPushButton { background: none; color: #555555; font-size: 13px; border: none; padding: 0; }"
              "QPushButton:hover { color: #1e1e2e; }");

    // Per-widget theme updates for panels with hardcoded inline styles
    if (m_buildBar)
        m_buildBar->applyTheme(m_isDarkTheme);
    if (m_runtimeStrip)
        m_runtimeStrip->applyTheme(m_isDarkTheme);
    if (m_clarityPanel)
        m_clarityPanel->applyTheme(m_isDarkTheme);
    if (m_aiChatPanel)
        m_aiChatPanel->applyTheme(m_isDarkTheme);
    if (m_levelSelector)
        m_levelSelector->applyTheme(m_isDarkTheme);

    // Update Windows title bar color via DWM
#ifdef Q_OS_WIN
    if (isVisible()) {
        HWND hwnd = reinterpret_cast<HWND>(winId());
        BOOL useDark = m_isDarkTheme ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd, 20, &useDark, sizeof(useDark));
        DwmSetWindowAttribute(hwnd, 19, &useDark, sizeof(useDark));
    }
#endif
}

// ── Session Persistence ─────────────────────────────────────────────────

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveSession();
    QMainWindow::closeEvent(event);
}

void MainWindow::saveSession()
{
    QSettings s("CodeClarity", "CodeClarity");

    // Window geometry
    s.setValue("geometry", saveGeometry());

    // Splitter sizes
    if (m_mainSplitter)
        s.setValue("mainSplitterSizes", QVariant::fromValue(m_mainSplitter->sizes()));
    if (m_rightSplitter)
        s.setValue("rightSplitterSizes", QVariant::fromValue(m_rightSplitter->sizes()));

    // Workspace paths (list for multi-workspace support)
    if (m_fileTree) {
        s.setValue("workspacePaths", m_fileTree->workspacePaths());
        // Keep legacy key for backwards compat
        s.setValue("workspacePath", m_fileTree->rootPath());
    }

    // Open files and active tab
    if (m_editor) {
        s.setValue("openFiles", m_editor->openFilePaths());
        s.setValue("activeTab", m_editor->activeTabIndex());
    }
}

void MainWindow::restoreSession()
{
    QSettings s("CodeClarity", "CodeClarity");

    // Window geometry
    if (s.contains("geometry"))
        restoreGeometry(s.value("geometry").toByteArray());

    // Splitter sizes
    if (s.contains("mainSplitterSizes") && m_mainSplitter) {
        auto sizes = s.value("mainSplitterSizes").value<QList<int>>();
        if (!sizes.isEmpty())
            m_mainSplitter->setSizes(sizes);
    }
    if (s.contains("rightSplitterSizes") && m_rightSplitter) {
        auto sizes = s.value("rightSplitterSizes").value<QList<int>>();
        if (!sizes.isEmpty())
            m_rightSplitter->setSizes(sizes);
    }

    // Workspace paths — only restore if user has previously opened a workspace.
    // On first-ever launch (no saved key), show the welcome page instead.
    bool hasWorkspaceSetting = s.contains("workspacePaths") || s.contains("workspacePath");
    if (hasWorkspaceSetting) {
        QStringList workspaces = s.value("workspacePaths").toStringList();
        if (workspaces.isEmpty()) {
            QString legacy = s.value("workspacePath").toString();
            if (!legacy.isEmpty())
                workspaces << legacy;
        }
        if (m_fileTree) {
            for (const QString& ws : workspaces) {
                if (QFileInfo::exists(ws))
                    m_fileTree->addWorkspaceFolder(ws);
            }
        }
    }

    // Re-open files
    QStringList files = s.value("openFiles").toStringList();
    for (const QString& path : files) {
        if (QFileInfo::exists(path) && m_editor)
            m_editor->openFile(path);
    }

    // Restore active tab
    int activeTab = s.value("activeTab", -1).toInt();
    if (activeTab >= 0 && m_editor)
        m_editor->setActiveTab(activeTab);
}

void MainWindow::addToRecentWorkspaces(const QString& path)
{
    QSettings s("CodeClarity", "CodeClarity");
    QStringList recent = s.value("recentWorkspaces").toStringList();

    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 5)
        recent.removeLast();

    s.setValue("recentWorkspaces", recent);
    rebuildRecentWorkspacesMenu();
}

void MainWindow::rebuildRecentWorkspacesMenu()
{
    if (!m_recentMenu) return;

    m_recentMenu->clear();

    QSettings s("CodeClarity", "CodeClarity");
    QStringList recent = s.value("recentWorkspaces").toStringList();

    if (recent.isEmpty()) {
        auto* empty = m_recentMenu->addAction("(No recent workspaces)");
        empty->setEnabled(false);
        return;
    }

    for (const QString& path : recent) {
        auto* action = m_recentMenu->addAction(path);
        connect(action, &QAction::triggered, this, [this, path]() {
            if (m_fileTree) {
                m_fileTree->setRootFolder(path);
                addToRecentWorkspaces(path);
            }
        });
    }
}
