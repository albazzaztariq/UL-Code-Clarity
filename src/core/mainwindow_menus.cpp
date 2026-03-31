#include "core/mainwindow.h"
#include "core/securitytesting.h"
#include "core/securitylab.h"
#include "core/runtimeanalysis.h"
#include "core/memoryanalysis.h"
#include "core/dependencyanalysis.h"
#include "core/codehealth.h"
#include "core/debugger.h"
#include "core/datatrace.h"
#include "core/errorjournal.h"
#include "core/costvisualizer.h"
#include "core/typeflow.h"
#include "core/machineview.h"
#include "core/settingspanel.h"
#include "core/custompipeline.h"
#include "core/editortracker.h"
#include <QTime>
#include "editor/editor.h"
#include "editor/filetree.h"
#include "panels/clarity.h"
#include "panels/aichat.h"
#include "panels/langguide.h"
#include "panels/basics.h"
#include "panels/expander.h"
#include "core/interactivebasics.h"
#include "core/tutorial.h"
#include "core/buildbar.h"
#include "core/predictpanel.h"
#include "core/runtimestrip.h"
#include "panels/levels.h"

#include <QMenuBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QAction>
#include <QFileDialog>
#include <QApplication>
#include <QFileInfo>
#include <QSettings>
#include <QPushButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QScrollArea>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QTextCursor>
#include <QTextDocument>
#include <QFrame>
#include "core/theme.h"

using namespace Theme::Css;

// ── Menu Bar ────────────────────────────────────────────────────────────
void MainWindow::createMenuBar()
{
    auto* mb = menuBar();

    // ── Helper: add an action with optional shortcut and connect it ───────
    auto addSimple = [&](QMenu* menu, const QString& text,
                         const QString& shortcut,
                         std::function<void()> cb) -> QAction* {
        auto* act = menu->addAction(text);
        if (!shortcut.isEmpty()) act->setShortcut(QKeySequence(shortcut));
        connect(act, &QAction::triggered, this, [cb]() { cb(); });
        return act;
    };

    // ── File ─────────────────────────────────────────────────────────────
    auto* fileMenu = mb->addMenu("&File");

    addSimple(fileMenu, "New File", "Ctrl+N", [this]() {
        if (m_editor) m_editor->newUntitled();
    });
    addSimple(fileMenu, "Open File...", "Ctrl+O", [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Open File", QString(),
            "All Files (*);;Python (*.py);;C (*.c *.h);;UniLogic (*.ul)");
        if (!path.isEmpty() && m_editor) m_editor->openFile(path);
    });
    addSimple(fileMenu, "Open Folder...", "Ctrl+Shift+O", [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Open Folder");
        if (!dir.isEmpty() && m_fileTree) {
            m_fileTree->setRootFolder(dir);
            addToRecentWorkspaces(dir);
        }
    });

    fileMenu->addSeparator();
    m_recentMenu = fileMenu->addMenu("Recent Workspaces");
    rebuildRecentWorkspacesMenu();
    fileMenu->addSeparator();

    addSimple(fileMenu, "Save",      "Ctrl+S",       [this]() { if (m_editor) m_editor->saveCurrentFile(); });
    addSimple(fileMenu, "Save As...", "Ctrl+Shift+S", [this]() { if (m_editor) m_editor->saveCurrentFileAs(); });

    fileMenu->addSeparator();

    addSimple(fileMenu, "Settings...", "Ctrl+,", [this]() {
        SettingsPanel dlg(this);
        connect(&dlg, &SettingsPanel::modelsChanged,        this, [this]() { if (m_aiChatPanel) m_aiChatPanel->reloadModelFromConfig(); });
        connect(&dlg, &SettingsPanel::menuVisibilityChanged, this, [this]() { applyToolsMenuVisibility(); });
        dlg.exec();
        if (m_aiChatPanel) m_aiChatPanel->reloadModelFromConfig();
    });

    fileMenu->addSeparator();
    connect(fileMenu->addAction("Exit"), &QAction::triggered, qApp, &QApplication::quit);

    // ── Edit ─────────────────────────────────────────────────────────────
    auto* editMenu = mb->addMenu("&Edit");

    addSimple(editMenu, "Undo", "Ctrl+Z", [this]() { if (m_editor) m_editor->undo(); });
    addSimple(editMenu, "Redo", "Ctrl+Y", [this]() { if (m_editor) m_editor->redo(); });
    editMenu->addSeparator();

    addSimple(editMenu, "Find", "Ctrl+F", [this]() {
        if (!m_editor) return;
        auto *dlg = new QDialog(this);
        dlg->setWindowTitle("Find");
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setMinimumWidth(340);
        auto *layout = new QVBoxLayout(dlg);
        layout->setSpacing(8);
        auto *row = new QHBoxLayout;
        auto *input = new QLineEdit(dlg);
        input->setPlaceholderText("Search text...");
        row->addWidget(new QLabel("Find:", dlg));
        row->addWidget(input, 1);
        layout->addLayout(row);
        auto *btnRow = new QHBoxLayout;
        auto *prevBtn  = new QPushButton("Previous", dlg);
        auto *nextBtn  = new QPushButton("Next", dlg);
        auto *closeBtn = new QPushButton("Close", dlg);
        btnRow->addWidget(prevBtn);
        btnRow->addWidget(nextBtn);
        btnRow->addStretch();
        btnRow->addWidget(closeBtn);
        layout->addLayout(btnRow);
        connect(nextBtn,  &QPushButton::clicked, dlg, [this, input]() { if (m_editor) m_editor->codeEditor()->find(input->text()); });
        connect(prevBtn,  &QPushButton::clicked, dlg, [this, input]() { if (m_editor) m_editor->codeEditor()->find(input->text(), QTextDocument::FindBackward); });
        connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
        connect(input, &QLineEdit::returnPressed, nextBtn, &QPushButton::click);
        dlg->show();
        input->setFocus();
    });

    // ── View ─────────────────────────────────────────────────────────────
    auto* viewMenu = mb->addMenu("&View");
    m_viewClarityAct = viewMenu->addAction("Show Clarity Panel");
    m_viewClarityAct->setCheckable(true);
    m_viewClarityAct->setChecked(true);
    connect(m_viewClarityAct, &QAction::toggled, this, [this](bool on) {
        if (m_clarityColumn) m_clarityColumn->setVisible(on);
    });

    m_viewAIChatAct = viewMenu->addAction("Show AI Chat Panel");
    m_viewAIChatAct->setCheckable(true);
    m_viewAIChatAct->setChecked(true);
    connect(m_viewAIChatAct, &QAction::toggled, this, [this](bool on) {
        if (m_chatColumn) m_chatColumn->setVisible(on);
    });

    viewMenu->addSeparator();
    auto *claudeCodeAct = viewMenu->addAction("Claude Code Mode");
    claudeCodeAct->setCheckable(true);
    claudeCodeAct->setChecked(false);
    connect(claudeCodeAct, &QAction::toggled, this, [this](bool on) {
        if (m_aiChatPanel) {
            m_aiChatPanel->setClaudeCodeMode(on);
            // Pass current workspace as working directory
            if (m_fileTree && !m_fileTree->rootPath().isEmpty())
                m_aiChatPanel->setWorkingDirectory(m_fileTree->rootPath());
            if (on && !m_editorTracker) {
                m_editorTracker = new EditorTracker(m_editor,
                    m_aiChatPanel->claudeBridge(), this);
                // Wire changelog to Clarity panel
                connect(m_editorTracker, &EditorTracker::fileModified, this,
                    [this](const QString &filePath, const QString &desc) {
                    if (m_clarityPanel) {
                        ClarityEntryData entry;
                        entry.title = desc;
                        entry.time = QTime::currentTime().toString("h:mm ap");
                        entry.detailHtml = QString("<b>File:</b> %1")
                            .arg(filePath.toHtmlEscaped());
                        m_clarityPanel->addEntry(entry);
                    }
                });
            }
        }
    });

    addSimple(editMenu, "Replace", "Ctrl+H", [this]() {
        if (!m_editor) return;
        auto *dlg = new QDialog(this);
        dlg->setWindowTitle("Find & Replace");
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setMinimumWidth(360);
        auto *layout = new QVBoxLayout(dlg);
        layout->setSpacing(8);
        auto *grid = new QGridLayout;
        auto *findInput = new QLineEdit(dlg);
        findInput->setPlaceholderText("Search text...");
        auto *replInput = new QLineEdit(dlg);
        replInput->setPlaceholderText("Replacement text...");
        grid->addWidget(new QLabel("Find:",    dlg), 0, 0); grid->addWidget(findInput, 0, 1);
        grid->addWidget(new QLabel("Replace:", dlg), 1, 0); grid->addWidget(replInput, 1, 1);
        layout->addLayout(grid);
        auto *btnRow   = new QHBoxLayout;
        auto *replNext = new QPushButton("Replace Next", dlg);
        auto *replAll  = new QPushButton("Replace All",  dlg);
        auto *closeBtn = new QPushButton("Close",        dlg);
        btnRow->addWidget(replNext);
        btnRow->addWidget(replAll);
        btnRow->addStretch();
        btnRow->addWidget(closeBtn);
        layout->addLayout(btnRow);
        connect(replNext, &QPushButton::clicked, dlg, [this, findInput, replInput]() {
            if (!m_editor) return;
            auto *ed = m_editor->codeEditor();
            if (ed->find(findInput->text())) { QTextCursor cur = ed->textCursor(); cur.insertText(replInput->text()); }
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

    // ── Learning ──────────────────────────────────────────────────────────
    createLearningMenu();

    // ── Tools ─────────────────────────────────────────────────────────────
    m_toolsMenu = mb->addMenu("&Tools");
    buildToolsMenu();

    // ── Help ──────────────────────────────────────────────────────────────
    auto* helpMenu = mb->addMenu("&Help");

    struct HelpDef { QString text; std::function<void()> cb; };
    const HelpDef helpItems[] = {
        { "Getting Started", [this]() {
            QMessageBox::information(this, "Getting Started",
                "1. Set up your AI model (File > Settings > Models)\n"
                "2. Open a folder or file\n"
                "3. Ask the AI to build something in the chat\n"
                "4. Read the Clarity panel to learn what happened\n"
                "5. Use Explain or Walk Me Through for deeper understanding");
        }},
        { "Keyboard Shortcuts", [this]() {
            QMessageBox::information(this, "Keyboard Shortcuts",
                "Ctrl+N    New File\n"
                "Ctrl+O    Open File\n"
                "Ctrl+Shift+O    Open Folder\n"
                "Ctrl+S    Save\n"
                "Ctrl+Shift+S    Save As\n"
                "Ctrl+,    Settings\n"
                "Ctrl+F    Find\n"
                "Ctrl+H    Replace");
        }},
        { "About Code Clarity", [this]() {
            QMessageBox::about(this, "About Code Clarity",
                "Code Clarity v0.1.0\n\n"
                "A code editor and learning platform for those new to programming or anyone looking to tie explicit learning to their coding work.\n\n"
                "https://github.com/albazzaztariq/UL-Code-Clarity\n\n"
                "Built with Qt 6.8.3\n"
                "© 2026 UniLogic Project");
        }},
    };
    for (const auto& h : helpItems) {
        auto cb = h.cb;
        connect(helpMenu->addAction(h.text), &QAction::triggered, this, [cb]() { cb(); });
    }

    helpMenu->addSeparator();
    connect(helpMenu->addAction("Report an Issue..."), &QAction::triggered, this, []() {
        QDesktopServices::openUrl(
            QUrl("https://github.com/albazzaztariq/UniLogic/issues/new?labels=code-clarity&template=bug_report.md&title=[Code+Clarity]+"));
    });
}

// ── Build the Tools menu (called on first create + after visibility changes) ──
void MainWindow::buildToolsMenu()
{
    m_toolsMenu->clear();
    m_toolsActions.clear();

    QSettings s("CodeClarity", "CodeClarity");

    // Current assist level (1=Beginner, 2=Intermediate, 3=Developer, 4=Expert)
    int level = m_levelSelector ? m_levelSelector->currentLevel() : 4;

    // Minimum level required for each tool key
    static const QMap<QString, int> toolMinLevel = {
        // Level 1: Beginner essentials
        {"langGuide",      1}, {"expander",        1}, {"debugger",    1},
        {"runtimeInfo",    1}, {"codeHealth",      1}, {"explainCodebase", 1},
        // Level 2: Intermediate
        {"errorJournal",   2}, {"typeFlow",        2},
        // Level 3: Developer
        {"securityTesting",3}, {"memoryAnalysis",   3}, {"depAnalysis", 3},
        {"runtimeAnalysis",3}, {"traceVar",         3},
        // Level 4: Expert / All
        {"securityLabs",   4}, {"machineView",      4}, {"execCost",   4},
        {"customPipeline", 4},
    };

    // Helper: register a visible/hidden tool action with optional shortcut.
    // Respects both menu visibility settings AND the current assist level.
    auto addToolAction = [&](const QString& name, const QString& key,
                              std::function<void()> handler,
                              const QString& shortcut = {}) -> QAction* {
        bool visible = s.value("menuvis/" + key, true).toBool();
        int minLevel = toolMinLevel.value(key, 1);
        auto* act = m_toolsMenu->addAction(name);
        if (!shortcut.isEmpty()) act->setShortcut(QKeySequence(shortcut));
        act->setVisible(visible && level >= minLevel);
        m_toolsActions[key] = act;
        connect(act, &QAction::triggered, this, [handler]() { handler(); });
        return act;
    };

    // Helper: hide all analysis panels and the main splitter.
    auto hideAll = [&]() {
        m_mainSplitter->setVisible(false);
        if (m_securityFrame)   m_securityFrame->setVisible(false);
        if (m_runtimeAnalysis) m_runtimeAnalysis->setVisible(false);
        if (m_memoryAnalysis)  m_memoryAnalysis->setVisible(false);
        if (m_depAnalysis)     m_depAnalysis->setVisible(false);
        if (m_codeHealth)      m_codeHealth->setVisible(false);
        if (m_debugFrame)      m_debugFrame->setVisible(false);
        if (m_dataTrace)       m_dataTrace->setVisible(false);
        if (m_errorJournal)    m_errorJournal->setVisible(false);
        if (m_typeFlow)        m_typeFlow->setVisible(false);
    };

    // ── Group 1: Utility tools ─────────────────────────────────────────────
    addToolAction("Language Guide", "langGuide", [this]() {
        if (m_langGuide && m_editor) m_langGuide->showForLanguage(m_editor->currentLanguage());
    });
    addToolAction("Statement Expander", "expander", [this]() {
        if (m_expanderOverlay) m_expanderOverlay->showDefaultDemo();
    });
    addToolAction("Explain Codebase", "explainCodebase", [this]() {
        if (!m_editor || !m_explainHandler) return;
        QString filePath = m_editor->currentFilePath();
        if (filePath.isEmpty()) return;
        QString filename = QFileInfo(filePath).fileName();
        if (m_explainHandler->hasExplanation(filename))
            m_editor->codeEditor()->setPlainText(m_explainHandler->getExplainedCode(filename));
    });

    m_toolsMenu->addSeparator();

    addToolAction("Runtime Info", "runtimeInfo", [this]() {
        if (m_runtimeStrip) m_runtimeStrip->toggle();
    }, "Ctrl+R");

    m_toolsMenu->addSeparator();

    // ── Group 2: Analysis panels ───────────────────────────────────────────
    struct AnalysisDef {
        QString label;
        QString key;
        QString shortcut;
        std::function<void()> cb;
    };

    const AnalysisDef analysisDefs[] = {
        { "Security Testing",     "securityTesting", "Ctrl+Shift+T", [this, hideAll]() {
            if (!m_securityFrame) return;
            QString code, lang, filePath;
            if (m_editor) { code = m_editor->currentContent(); lang = currentLangKey(); filePath = m_editor->currentFilePath(); }
            m_securityFrame->setCode(code, lang, filePath);
            hideAll(); m_securityFrame->setVisible(true);
        }},
        { "Runtime Analysis...",  "runtimeAnalysis", "Ctrl+Shift+R", [this, hideAll]() {
            if (!m_runtimeAnalysis) return;
            hideAll(); m_runtimeAnalysis->setVisible(true);
        }},
        { "Memory Analysis...",   "memoryAnalysis",  "Ctrl+Shift+M", [this, hideAll]() {
            if (!m_memoryAnalysis) return;
            QString sourceFile, binaryFile, lang;
            if (m_editor) { sourceFile = m_editor->currentFilePath(); lang = currentLangKey(); }
            m_memoryAnalysis->setFileInfo(sourceFile, binaryFile, lang);
            hideAll(); m_memoryAnalysis->setVisible(true);
        }},
        { "Dependency Analysis...", "depAnalysis",   "Ctrl+Shift+D", [this, hideAll]() {
            if (!m_depAnalysis) return;
            QString code, lang, filePath;
            if (m_editor) { code = m_editor->currentContent(); lang = currentLangKey(); filePath = m_editor->currentFilePath(); }
            m_depAnalysis->setCode(code, lang, filePath);
            hideAll(); m_depAnalysis->setVisible(true);
        }},
    };

    for (const auto& d : analysisDefs)
        addToolAction(d.label, d.key, d.cb, d.shortcut);

    m_toolsMenu->addSeparator();

    // ── Security Labs (special: dialog with lab picker) ────────────────────
    addToolAction("Security Labs...", "securityLabs", [this]() {
        auto* dlg = new QDialog(this);
        dlg->setWindowTitle("Security Labs");
        dlg->setModal(true);
        dlg->setMinimumWidth(440);
        dlg->setStyleSheet(QString("background: %1; color: %2;").arg(BG, FG));

        auto* lay = new QVBoxLayout(dlg);
        lay->setContentsMargins(20, 16, 20, 16);
        lay->setSpacing(10);

        auto* titleLbl = new QLabel("Interactive Security Labs");
        titleLbl->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: bold;").arg(FG));
        lay->addWidget(titleLbl);

        auto* sub = new QLabel("Experience vulnerabilities first-hand. Type the attack input and\n"
                               "watch what happens — then see the fix.");
        sub->setStyleSheet(QString("color: %1; font-size: 12px;").arg(FG2));
        lay->addWidget(sub);

        auto* sep = new QFrame;
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet(QString("color: %1;").arg(BORDER));
        lay->addWidget(sep);

        const QString labBtnCss = QString(
            "QPushButton { background: %1; color: %2; border: 1px solid %3;"
            " border-radius: 4px; padding: 0 14px; font-size: 13px; text-align: left; }"
            "QPushButton:hover { background: %4; }").arg(BG3, FG, BORDER, BG4);

        for (const auto& lab : SecurityLabWidget::allLabs()) {
            auto* btn = new QPushButton(lab.name);
            btn->setFixedHeight(34);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(labBtnCss);
            QString vt = lab.vulnType;
            connect(btn, &QPushButton::clicked, dlg, [this, dlg, vt]() {
                dlg->accept();
                if (m_securityLab) { m_securityLab->setVisible(false); m_securityLab->deleteLater(); m_securityLab = nullptr; }
                m_securityLab = SecurityLabWidget::forVulnType(vt, centralWidget());
                if (!m_securityLab) return;
                auto* cl = qobject_cast<QVBoxLayout*>(centralWidget()->layout());
                if (cl) cl->insertWidget(cl->count() - 1, m_securityLab);
                connect(m_securityLab, &SecurityLabWidget::backToEditor, this, [this]() {
                    if (m_securityLab) m_securityLab->setVisible(false);
                    m_mainSplitter->setVisible(true);
                });
                m_mainSplitter->setVisible(false);
                if (m_securityFrame)   m_securityFrame->setVisible(false);
                if (m_runtimeAnalysis) m_runtimeAnalysis->setVisible(false);
                if (m_memoryAnalysis)  m_memoryAnalysis->setVisible(false);
                if (m_depAnalysis)     m_depAnalysis->setVisible(false);
                m_securityLab->setVisible(true);
            });
            lay->addWidget(btn);
        }

        const QString closeBtnCss = QString(
            "QPushButton { background: %1; color: %2; border: 1px solid %3;"
            " border-radius: 6px; padding: 0 18px; font-size: 13px; min-height: 30px; }"
            "QPushButton:hover { background: %4; }").arg(BG3, FG, BORDER, BG4);
        auto* closeBtn = new QPushButton("Close");
        closeBtn->setStyleSheet(closeBtnCss);
        closeBtn->setCursor(Qt::PointingHandCursor);
        connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
        auto* btnRow = new QHBoxLayout;
        btnRow->addStretch();
        btnRow->addWidget(closeBtn);
        lay->addLayout(btnRow);

        dlg->exec();
        dlg->deleteLater();
    }, "Ctrl+Shift+L");

    m_toolsMenu->addSeparator();

    addToolAction("Code Health", "codeHealth", [this, hideAll]() {
        if (!m_codeHealth) return;
        QString code, lang;
        if (m_editor) { code = m_editor->currentContent(); lang = currentLangKey(); }
        m_codeHealth->setCode(code, lang);
        hideAll(); m_codeHealth->setVisible(true);
    }, "Ctrl+Shift+H");

    m_toolsMenu->addSeparator();

    addToolAction("Debugger", "debugger", [this, hideAll]() {
        if (!m_debugFrame || !m_editor) return;
        QString filePath = m_editor->currentFilePath();
        if (filePath.isEmpty()) {
            QMessageBox::information(this, "Save First", "Please save your file before debugging.");
            return;
        }
        m_editor->saveCurrentFile();
        m_debugFrame->setTargetFile(filePath, currentLangKey());
        hideAll(); m_debugFrame->setVisible(true);
        m_debugFrame->startDebugging();
    }, "F5");

    m_toolsMenu->addSeparator();

    addToolAction("Trace This Variable...", "traceVar", [this, hideAll]() {
        if (!m_dataTrace || !m_editor) return;
        QString code     = m_editor->currentContent();
        QString lang     = currentLangKey();
        QString filePath = m_editor->currentFilePath();
        QString selected = m_editor->codeEditor()->textCursor().selectedText().trimmed();
        m_dataTrace->setCode(code, lang, filePath);
        if (!selected.isEmpty()) m_dataTrace->setVariable(selected);
        hideAll(); m_dataTrace->setVisible(true);
    }, "Ctrl+Shift+V");

    addToolAction("Error Journal", "errorJournal", [this, hideAll]() {
        if (!m_errorJournal) return;
        if (m_fileTree && !m_fileTree->rootPath().isEmpty())
            m_errorJournal->setWorkspaceRoot(m_fileTree->rootPath());
        hideAll(); m_errorJournal->setVisible(true);
    }, "Ctrl+Shift+E");

    m_toolsMenu->addSeparator();

    addToolAction("Type Flow...", "typeFlow", [this, hideAll]() {
        if (!m_typeFlow || !m_editor) return;
        m_typeFlow->setCode(m_editor->currentContent(), currentLangKey());
        hideAll(); m_typeFlow->setVisible(true);
    }, "Ctrl+Shift+F");

    // ── Machine View (checkable) ───────────────────────────────────────────
    {
        auto* machineViewAction = m_toolsMenu->addAction("Machine View");
        machineViewAction->setShortcut(QKeySequence("Ctrl+Shift+W"));
        machineViewAction->setCheckable(true);
        machineViewAction->setVisible(s.value("menuvis/machineView", true).toBool()
                                      && level >= toolMinLevel.value("machineView", 4));
        m_toolsActions["machineView"] = machineViewAction;
        connect(machineViewAction, &QAction::triggered, this,
            [this, machineViewAction](bool checked) {
                if (!m_machineView) return;
                if (checked) {
                    if (m_levelSelector) m_machineView->setLevel(m_levelSelector->currentLevel());
                    if (m_editor)        m_machineView->setLanguage(currentLangKey());
                    m_machineView->clearExplanation();
                    m_machineView->setVisible(true);
                } else {
                    m_machineView->setVisible(false);
                }
                Q_UNUSED(machineViewAction);
            });
    }

    m_toolsMenu->addSeparator();

    // ── Execution Cost Visualizer (checkable) ──────────────────────────────
    {
        auto* costAction = m_toolsMenu->addAction("Show Execution Cost");
        costAction->setShortcut(QKeySequence("Ctrl+Shift+C"));
        costAction->setCheckable(true);
        costAction->setVisible(s.value("menuvis/execCost", true).toBool()
                               && level >= toolMinLevel.value("execCost", 4));
        m_toolsActions["execCost"] = costAction;
        connect(costAction, &QAction::triggered, this,
            [this, costAction](bool checked) {
                if (!m_editor) return;
                if (checked) {
                    QString filePath = m_editor->currentFilePath();
                    if (filePath.isEmpty()) {
                        costAction->setChecked(false);
                        QMessageBox::information(this, "Save First",
                            "Please save your file before profiling execution cost.");
                        return;
                    }
                    if (currentLangKey() != "python") {
                        costAction->setChecked(false);
                        QMessageBox::information(this, "Python Only",
                            "Execution Cost Visualizer currently supports Python files only.");
                        return;
                    }
                    m_editor->saveCurrentFile();
                    if (!m_costVisualizer) {
                        m_costVisualizer = new CostVisualizer(this);
                        m_editor->codeEditor()->setCostVisualizer(m_costVisualizer);
                        connect(m_costVisualizer, &CostVisualizer::statusMessage, this,
                            [this](const QString& msg) { if (m_costPanel) m_costPanel->setStatus(msg); });
                        connect(m_costVisualizer, &CostVisualizer::errorOccurred, this,
                            [this, costAction](const QString& err) {
                                QMessageBox::warning(this, "Profiler Error", err);
                                if (m_costPanel) m_costPanel->setVisible(false);
                                costAction->setChecked(false);
                            });
                    }
                    if (!m_costPanel) {
                        m_costPanel = new CostVisualizerPanel(m_editor);
                        auto* editorLayout = qobject_cast<QVBoxLayout*>(m_editor->layout());
                        if (editorLayout) editorLayout->insertWidget(0, m_costPanel);
                        connect(m_costPanel, &CostVisualizerPanel::stopRequested, this,
                            [this, costAction]() {
                                if (m_costVisualizer) m_costVisualizer->clear();
                                if (m_costPanel) m_costPanel->setVisible(false);
                                if (m_editor) m_editor->codeEditor()->viewport()->update();
                                costAction->setChecked(false);
                            });
                    }
                    m_costPanel->setVisible(true);
                    m_costPanel->setStatus("Profiling...");
                    m_costVisualizer->profileFile(filePath);
                } else {
                    if (m_costVisualizer) m_costVisualizer->clear();
                    if (m_costPanel) m_costPanel->setVisible(false);
                    if (m_editor) m_editor->codeEditor()->viewport()->update();
                }
            });
    }

    m_toolsMenu->addSeparator();

    // ── Custom Pipeline (Level 4 only) ──────────────────────────────────────
    auto* runPipelineAction = m_toolsMenu->addAction("Run Custom Pipeline...");
    runPipelineAction->setVisible(level >= toolMinLevel.value("customPipeline", 4));
    m_toolsActions["customPipeline"] = runPipelineAction;
    connect(runPipelineAction, &QAction::triggered, this, [this]() {
        auto* dlg = new CustomPipelineDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(dlg, &CustomPipelineDialog::pipelineSaved, this, [this]() { rebuildSavedPipelinesMenu(); });
        connect(dlg, &CustomPipelineDialog::runRequested,  this, [this](const PipelineConfig& cfg) { runPipeline(cfg); });
        dlg->exec();
    });

    m_savedPipelinesMenu = m_toolsMenu->addMenu("Saved Pipelines");
    m_savedPipelinesMenu->menuAction()->setVisible(level >= toolMinLevel.value("customPipeline", 4));
    rebuildSavedPipelinesMenu();
}

// ── Rebuild Saved Pipelines submenu ──────────────────────────────────────
void MainWindow::rebuildSavedPipelinesMenu()
{
    if (!m_savedPipelinesMenu) return;
    m_savedPipelinesMenu->clear();

    QStringList pipelines = PipelineConfig::savedPipelineNames();
    if (pipelines.isEmpty()) {
        auto* empty = m_savedPipelinesMenu->addAction("(No saved pipelines)");
        empty->setEnabled(false);
        return;
    }

    for (const QString& name : pipelines) {
        auto* act = m_savedPipelinesMenu->addAction(name);
        connect(act, &QAction::triggered, this, [this, name]() {
            PipelineConfig cfg = PipelineConfig::load(name);
            runPipeline(cfg);
        });
    }
}

// ── Learning menu ─────────────────────────────────────────────────────────
void MainWindow::createLearningMenu()
{
    auto* learningMenu = menuBar()->addMenu("&Learning");

    // Helper: create tutorial dialogs via a factory function.
    auto addTutorial = [&](const QString& label, std::function<QDialog*(QWidget*)> factory) {
        connect(learningMenu->addAction(label), &QAction::triggered, this, [this, factory]() {
            auto* dlg = factory(this);
            dlg->exec();
            dlg->deleteLater();
        });
    };

    // ── Tutorials section ─────────────────────────────────────────────────
    connect(learningMenu->addAction("Interactive Basics"), &QAction::triggered, this, [this]() {
        if (!m_interactiveBasics) m_interactiveBasics = new InteractiveBasicsDialog(this);
        m_interactiveBasics->exec();
    });

    addTutorial("Build Target Tutorial", [](QWidget* p) { return TutorialDialog::buildTarget(p); });
    addTutorial("Memory Tutorial",       [](QWidget* p) { return TutorialDialog::memory(p); });
    addTutorial("Security Tutorial",     [](QWidget* p) { return TutorialDialog::security(p); });
    addTutorial("Code Health Tutorial",  [](QWidget* p) { return TutorialDialog::codeHealth(p); });

    connect(learningMenu->addAction("Quick Reference"), &QAction::triggered, this, [this]() {
        if (m_basicsOverlay) {
            m_basicsOverlay->goToPage(0);
            m_basicsOverlay->setVisible(true);
            m_basicsOverlay->raise();
        }
    });

    // ── Games section ─────────────────────────────────────────────────────
    learningMenu->addSeparator();

    connect(learningMenu->addAction("Debug Game"), &QAction::triggered, this, [this]() {
        if (!m_interactiveBasics) m_interactiveBasics = new InteractiveBasicsDialog(this);
        m_interactiveBasics->exec();
    });
    connect(learningMenu->addAction("Predict Before You Run"), &QAction::triggered, this, [this]() {
        if (m_buildBar)     emit m_buildBar->predictToggled(true);
        if (m_predictPanel) m_predictPanel->setActive(true);
    });
}

// ── Apply Tools menu visibility from QSettings ────────────────────────────
void MainWindow::applyToolsMenuVisibility()
{
    QSettings s("CodeClarity", "CodeClarity");
    for (auto it = m_toolsActions.begin(); it != m_toolsActions.end(); ++it) {
        bool visible = s.value("menuvis/" + it.key(), true).toBool();
        it.value()->setVisible(visible);
    }
}

// ── Run a pipeline ────────────────────────────────────────────────────────
void MainWindow::runPipeline(const PipelineConfig& cfg)
{
    if (!m_editor) return;
    QString code     = m_editor->currentContent();
    QString lang     = currentLangKey();
    QString filePath = m_editor->currentFilePath();

    auto* runner = new PipelineRunner(cfg, this);
    connect(runner, &PipelineRunner::finished, this,
        [this, runner](const QList<PipelineResult>& results) {
            // Show results in a dialog
            auto* dlg = new QDialog(this);
            dlg->setWindowTitle("Pipeline Results");
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->setMinimumSize(520, 400);
            dlg->setStyleSheet(QString("QDialog { background: %1; color: %2; }").arg(BG, FG));
            auto* lay = new QVBoxLayout(dlg);
            lay->setContentsMargins(16, 16, 16, 16);
            lay->setSpacing(8);

            auto* title = new QLabel("Pipeline Results");
            title->setStyleSheet(QString("font-size: 15px; font-weight: bold; color: %1;").arg(FG));
            lay->addWidget(title);

            auto* scroll = new QScrollArea(dlg);
            scroll->setWidgetResizable(true);
            scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");
            auto* inner = new QWidget;
            auto* innerLay = new QVBoxLayout(inner);
            innerLay->setSpacing(6);

            for (const auto& r : results) {
                auto* row = new QWidget;
                row->setStyleSheet(QString("background: %1; border-radius: 4px; padding: 6px;").arg(BG2));
                auto* rowLay = new QVBoxLayout(row);
                rowLay->setContentsMargins(8, 6, 8, 6);
                rowLay->setSpacing(2);

                auto* header = new QLabel(QString("<b>%1</b> — %2")
                    .arg(r.checkName.toHtmlEscaped(),
                         r.passed ? QString("<span style='color:%1;'>Passed</span>").arg(GREEN)
                                  : QString("<span style='color:%1;'>Issues found</span>").arg(RED)));
                header->setStyleSheet(QString("color: %1; font-size: 12px;").arg(FG));
                rowLay->addWidget(header);

                if (!r.summary.isEmpty()) {
                    auto* sumLbl = new QLabel(r.summary);
                    sumLbl->setWordWrap(true);
                    sumLbl->setStyleSheet(QString("color: %1; font-size: 11px;").arg(FG2));
                    rowLay->addWidget(sumLbl);
                }

                innerLay->addWidget(row);
            }

            innerLay->addStretch();
            scroll->setWidget(inner);
            lay->addWidget(scroll, 1);

            auto* closeBtn = new QPushButton("Close");
            closeBtn->setStyleSheet(QString(
                "QPushButton { background: %1; color: %2; border: 1px solid %3;"
                " border-radius: 4px; padding: 6px 18px; font-size: 12px; }"
                "QPushButton:hover { background: %4; }").arg(BG3, FG, BORDER, BG4));
            connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
            auto* btnRow = new QHBoxLayout;
            btnRow->addStretch();
            btnRow->addWidget(closeBtn);
            lay->addLayout(btnRow);

            dlg->exec();
            runner->deleteLater();
        });

    runner->run(code, lang, filePath);
}
