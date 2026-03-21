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

// ── Menu Bar ────────────────────────────────────────────────────────────
void MainWindow::createMenuBar()
{
    auto* mb = menuBar();

    // ── File ─────────────────────────────────────────────────────────────
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
        connect(&dlg, &SettingsPanel::menuVisibilityChanged, this, [this]() {
            applyToolsMenuVisibility();
        });
        dlg.exec();
        if (m_aiChatPanel) m_aiChatPanel->reloadModelFromConfig();
    });

    fileMenu->addSeparator();

    auto* exit = fileMenu->addAction("Exit");
    connect(exit, &QAction::triggered, qApp, &QApplication::quit);

    // ── Edit ─────────────────────────────────────────────────────────────
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

    // ── Learning ──────────────────────────────────────────────────────────
    createLearningMenu();

    // ── Tools ─────────────────────────────────────────────────────────────
    m_toolsMenu = mb->addMenu("&Tools");
    buildToolsMenu();

    // ── Help ──────────────────────────────────────────────────────────────
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

// ── Build the Tools menu (called on first create + after visibility changes) ──
void MainWindow::buildToolsMenu()
{
    m_toolsMenu->clear();
    m_toolsActions.clear();

    QSettings s("CodeClarity", "CodeClarity");

    auto addToolAction = [&](const QString& name, const QString& settingsKey,
                              std::function<void()> handler,
                              const QKeySequence& shortcut = {},
                              bool checkable = false) -> QAction* {
        bool visible = s.value("menuvis/" + settingsKey, true).toBool();
        auto* act = m_toolsMenu->addAction(name);
        if (!shortcut.isEmpty()) act->setShortcut(shortcut);
        if (checkable) act->setCheckable(true);
        act->setVisible(visible);
        m_toolsActions[settingsKey] = act;
        connect(act, &QAction::triggered, this, [handler]() { handler(); });
        return act;
    };

    addToolAction("Language Guide", "langGuide", [this]() {
        if (m_langGuide && m_editor)
            m_langGuide->showForLanguage(m_editor->currentLanguage());
    });

    addToolAction("Statement Expander", "expander", [this]() {
        if (m_expanderOverlay) m_expanderOverlay->showDefaultDemo();
    });

    addToolAction("Explain Codebase", "explainCodebase", [this]() {
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

    m_toolsMenu->addSeparator();

    addToolAction("Runtime Info", "runtimeInfo", [this]() {
        if (m_runtimeStrip) m_runtimeStrip->toggle();
    }, QKeySequence("Ctrl+R"));

    m_toolsMenu->addSeparator();

    addToolAction("Security Testing", "securityTesting", [this]() {
        if (!m_securityFrame) return;
        QString code, lang, filePath;
        if (m_editor) {
            code     = m_editor->currentContent();
            lang     = currentLangKey();
            filePath = m_editor->currentFilePath();
        }
        m_securityFrame->setCode(code, lang, filePath);
        m_mainSplitter->setVisible(false);
        m_securityFrame->setVisible(true);
    }, QKeySequence("Ctrl+Shift+T"));

    addToolAction("Runtime Analysis...", "runtimeAnalysis", [this]() {
        if (!m_runtimeAnalysis) return;
        m_mainSplitter->setVisible(false);
        if (m_securityFrame) m_securityFrame->setVisible(false);
        if (m_memoryAnalysis) m_memoryAnalysis->setVisible(false);
        if (m_depAnalysis) m_depAnalysis->setVisible(false);
        m_runtimeAnalysis->setVisible(true);
    }, QKeySequence("Ctrl+Shift+R"));

    addToolAction("Memory Analysis...", "memoryAnalysis", [this]() {
        if (!m_memoryAnalysis) return;
        QString sourceFile, binaryFile, lang;
        if (m_editor) {
            sourceFile = m_editor->currentFilePath();
            lang       = currentLangKey();
        }
        m_memoryAnalysis->setFileInfo(sourceFile, binaryFile, lang);
        m_mainSplitter->setVisible(false);
        if (m_securityFrame) m_securityFrame->setVisible(false);
        if (m_runtimeAnalysis) m_runtimeAnalysis->setVisible(false);
        if (m_depAnalysis) m_depAnalysis->setVisible(false);
        m_memoryAnalysis->setVisible(true);
    }, QKeySequence("Ctrl+Shift+M"));

    addToolAction("Dependency Analysis...", "depAnalysis", [this]() {
        if (!m_depAnalysis) return;
        QString code, lang, filePath;
        if (m_editor) {
            code     = m_editor->currentContent();
            lang     = currentLangKey();
            filePath = m_editor->currentFilePath();
        }
        m_depAnalysis->setCode(code, lang, filePath);
        m_mainSplitter->setVisible(false);
        if (m_securityFrame) m_securityFrame->setVisible(false);
        if (m_runtimeAnalysis) m_runtimeAnalysis->setVisible(false);
        if (m_memoryAnalysis) m_memoryAnalysis->setVisible(false);
        m_depAnalysis->setVisible(true);
    }, QKeySequence("Ctrl+Shift+D"));

    m_toolsMenu->addSeparator();

    addToolAction("Security Labs...", "securityLabs", [this]() {
        auto* dlg = new QDialog(this);
        dlg->setWindowTitle("Security Labs");
        dlg->setModal(true);
        dlg->setMinimumWidth(440);
        dlg->setStyleSheet("background: #1e1e2e; color: #cdd6f4;");

        auto* lay = new QVBoxLayout(dlg);
        lay->setContentsMargins(20, 16, 20, 16);
        lay->setSpacing(10);

        auto* title = new QLabel("Interactive Security Labs");
        title->setStyleSheet("color: #cdd6f4; font-size: 16px; font-weight: bold;");
        lay->addWidget(title);

        auto* sub = new QLabel(
            "Experience vulnerabilities first-hand. Type the attack input and\n"
            "watch what happens — then see the fix.");
        sub->setStyleSheet("color: #a6adc8; font-size: 12px;");
        lay->addWidget(sub);

        auto* sep = new QFrame;
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet("color: #313244;");
        lay->addWidget(sep);

        for (const auto& lab : SecurityLabWidget::allLabs()) {
            auto* btn = new QPushButton(lab.name);
            btn->setFixedHeight(34);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(
                "QPushButton { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
                " border-radius: 4px; padding: 0 14px; font-size: 13px; text-align: left; }"
                "QPushButton:hover { background: #45475a; }");
            QString vt = lab.vulnType;
            connect(btn, &QPushButton::clicked, dlg, [this, dlg, vt]() {
                dlg->accept();
                if (m_securityLab) {
                    m_securityLab->setVisible(false);
                    m_securityLab->deleteLater();
                    m_securityLab = nullptr;
                }
                m_securityLab = SecurityLabWidget::forVulnType(vt, centralWidget());
                if (!m_securityLab) return;
                auto* cl = qobject_cast<QVBoxLayout*>(centralWidget()->layout());
                if (cl) cl->insertWidget(cl->count() - 1, m_securityLab);
                connect(m_securityLab, &SecurityLabWidget::closeRequested, this, [this]() {
                    if (m_securityLab) m_securityLab->setVisible(false);
                    m_mainSplitter->setVisible(true);
                });
                m_mainSplitter->setVisible(false);
                if (m_securityFrame) m_securityFrame->setVisible(false);
                if (m_runtimeAnalysis) m_runtimeAnalysis->setVisible(false);
                if (m_memoryAnalysis) m_memoryAnalysis->setVisible(false);
                if (m_depAnalysis) m_depAnalysis->setVisible(false);
                m_securityLab->setVisible(true);
            });
            lay->addWidget(btn);
        }

        auto* closeBtn = new QPushButton("Close");
        closeBtn->setStyleSheet(
            "QPushButton { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
            " border-radius: 6px; padding: 0 18px; font-size: 13px; min-height: 30px; }"
            "QPushButton:hover { background: #45475a; }");
        closeBtn->setCursor(Qt::PointingHandCursor);
        connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
        auto* btnRow = new QHBoxLayout;
        btnRow->addStretch();
        btnRow->addWidget(closeBtn);
        lay->addLayout(btnRow);

        dlg->exec();
        dlg->deleteLater();
    }, QKeySequence("Ctrl+Shift+L"));

    m_toolsMenu->addSeparator();

    addToolAction("Code Health", "codeHealth", [this]() {
        if (!m_codeHealth) return;
        QString code, lang;
        if (m_editor) {
            code = m_editor->currentContent();
            lang = currentLangKey();
        }
        m_codeHealth->setCode(code, lang);
        m_mainSplitter->setVisible(false);
        if (m_securityFrame)    m_securityFrame->setVisible(false);
        if (m_runtimeAnalysis)  m_runtimeAnalysis->setVisible(false);
        if (m_memoryAnalysis)   m_memoryAnalysis->setVisible(false);
        if (m_depAnalysis)      m_depAnalysis->setVisible(false);
        m_codeHealth->setVisible(true);
    }, QKeySequence("Ctrl+Shift+H"));

    m_toolsMenu->addSeparator();

    addToolAction("Debugger", "debugger", [this]() {
        if (!m_debugFrame || !m_editor) return;
        QString filePath = m_editor->currentFilePath();
        if (filePath.isEmpty()) {
            QMessageBox::information(this, "Save First",
                "Please save your file before debugging.");
            return;
        }
        m_editor->saveCurrentFile();
        m_debugFrame->setTargetFile(filePath, currentLangKey());
        m_mainSplitter->setVisible(false);
        if (m_securityFrame)    m_securityFrame->setVisible(false);
        if (m_runtimeAnalysis)  m_runtimeAnalysis->setVisible(false);
        if (m_memoryAnalysis)   m_memoryAnalysis->setVisible(false);
        if (m_depAnalysis)      m_depAnalysis->setVisible(false);
        if (m_codeHealth)       m_codeHealth->setVisible(false);
        m_debugFrame->setVisible(true);
        m_debugFrame->startDebugging();
    }, QKeySequence("F5"));

    m_toolsMenu->addSeparator();

    addToolAction("Trace This Variable...", "traceVar", [this]() {
        if (!m_dataTrace || !m_editor) return;
        QString code     = m_editor->currentContent();
        QString lang     = currentLangKey();
        QString filePath = m_editor->currentFilePath();
        QString selected = m_editor->codeEditor()->textCursor().selectedText().trimmed();
        m_dataTrace->setCode(code, lang, filePath);
        if (!selected.isEmpty())
            m_dataTrace->setVariable(selected);
        m_mainSplitter->setVisible(false);
        if (m_securityFrame)    m_securityFrame->setVisible(false);
        if (m_runtimeAnalysis)  m_runtimeAnalysis->setVisible(false);
        if (m_memoryAnalysis)   m_memoryAnalysis->setVisible(false);
        if (m_depAnalysis)      m_depAnalysis->setVisible(false);
        if (m_codeHealth)       m_codeHealth->setVisible(false);
        if (m_debugFrame)       m_debugFrame->setVisible(false);
        m_dataTrace->setVisible(true);
    }, QKeySequence("Ctrl+Shift+V"));

    addToolAction("Error Journal", "errorJournal", [this]() {
        if (!m_errorJournal) return;
        if (m_fileTree && !m_fileTree->rootPath().isEmpty())
            m_errorJournal->setWorkspaceRoot(m_fileTree->rootPath());
        m_mainSplitter->setVisible(false);
        if (m_securityFrame)    m_securityFrame->setVisible(false);
        if (m_runtimeAnalysis)  m_runtimeAnalysis->setVisible(false);
        if (m_memoryAnalysis)   m_memoryAnalysis->setVisible(false);
        if (m_depAnalysis)      m_depAnalysis->setVisible(false);
        if (m_codeHealth)       m_codeHealth->setVisible(false);
        if (m_debugFrame)       m_debugFrame->setVisible(false);
        if (m_dataTrace)        m_dataTrace->setVisible(false);
        m_errorJournal->setVisible(true);
    }, QKeySequence("Ctrl+Shift+E"));

    m_toolsMenu->addSeparator();

    addToolAction("Type Flow...", "typeFlow", [this]() {
        if (!m_typeFlow || !m_editor) return;
        QString code = m_editor->currentContent();
        QString lang = currentLangKey();
        m_typeFlow->setCode(code, lang);
        m_mainSplitter->setVisible(false);
        if (m_securityFrame)    m_securityFrame->setVisible(false);
        if (m_runtimeAnalysis)  m_runtimeAnalysis->setVisible(false);
        if (m_memoryAnalysis)   m_memoryAnalysis->setVisible(false);
        if (m_depAnalysis)      m_depAnalysis->setVisible(false);
        if (m_codeHealth)       m_codeHealth->setVisible(false);
        if (m_debugFrame)       m_debugFrame->setVisible(false);
        if (m_dataTrace)        m_dataTrace->setVisible(false);
        if (m_errorJournal)     m_errorJournal->setVisible(false);
        m_typeFlow->setVisible(true);
    }, QKeySequence("Ctrl+Shift+F"));

    // Machine View — checkable, handled specially
    {
        bool visible = s.value("menuvis/machineView", true).toBool();
        auto* machineViewAction = m_toolsMenu->addAction("Machine View");
        machineViewAction->setShortcut(QKeySequence("Ctrl+Shift+W"));
        machineViewAction->setCheckable(true);
        machineViewAction->setVisible(visible);
        m_toolsActions["machineView"] = machineViewAction;
        connect(machineViewAction, &QAction::triggered, this,
            [this, machineViewAction](bool checked) {
                if (!m_machineView) return;
                if (checked) {
                    if (m_levelSelector)
                        m_machineView->setLevel(m_levelSelector->currentLevel());
                    if (m_editor)
                        m_machineView->setLanguage(currentLangKey());
                    m_machineView->clearExplanation();
                    m_machineView->setVisible(true);
                } else {
                    m_machineView->setVisible(false);
                }
                Q_UNUSED(machineViewAction);
            });
    }

    m_toolsMenu->addSeparator();

    // Execution Cost Visualizer — checkable, handled specially
    {
        bool visible = s.value("menuvis/execCost", true).toBool();
        auto* costAction = m_toolsMenu->addAction("Show Execution Cost");
        costAction->setShortcut(QKeySequence("Ctrl+Shift+C"));
        costAction->setCheckable(true);
        costAction->setVisible(visible);
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
                    QString lang = currentLangKey();
                    if (lang != "python") {
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
                            [this](const QString& msg) {
                                if (m_costPanel) m_costPanel->setStatus(msg);
                            });
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

    // ── Custom Pipeline ────────────────────────────────────────────────────
    auto* runPipelineAction = m_toolsMenu->addAction("Run Custom Pipeline...");
    m_toolsActions["customPipeline"] = runPipelineAction;
    connect(runPipelineAction, &QAction::triggered, this, [this]() {
        auto* dlg = new CustomPipelineDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(dlg, &CustomPipelineDialog::pipelineSaved, this, [this]() {
            rebuildSavedPipelinesMenu();
        });
        connect(dlg, &CustomPipelineDialog::runRequested, this,
            [this](const PipelineConfig& cfg) {
                runPipeline(cfg);
            });
        dlg->exec();
    });

    // Saved Pipelines submenu
    m_savedPipelinesMenu = m_toolsMenu->addMenu("Saved Pipelines");
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
    auto* mb = menuBar();
    auto* learningMenu = mb->addMenu("&Learning");

    // ── Tutorials section ─────────────────────────────────────────────────
    auto* interactiveTut = learningMenu->addAction("Interactive Basics");
    connect(interactiveTut, &QAction::triggered, this, [this]() {
        if (!m_interactiveBasics) m_interactiveBasics = new InteractiveBasicsDialog(this);
        m_interactiveBasics->exec();
    });

    auto* buildTargetTut = learningMenu->addAction("Build Target Tutorial");
    connect(buildTargetTut, &QAction::triggered, this, [this]() {
        auto* dlg = TutorialDialog::buildTarget(this);
        dlg->exec();
        dlg->deleteLater();
    });

    auto* memTut = learningMenu->addAction("Memory Tutorial");
    connect(memTut, &QAction::triggered, this, [this]() {
        auto* dlg = TutorialDialog::memory(this);
        dlg->exec();
        dlg->deleteLater();
    });

    auto* secTut = learningMenu->addAction("Security Tutorial");
    connect(secTut, &QAction::triggered, this, [this]() {
        auto* dlg = TutorialDialog::security(this);
        dlg->exec();
        dlg->deleteLater();
    });

    auto* healthTut = learningMenu->addAction("Code Health Tutorial");
    connect(healthTut, &QAction::triggered, this, [this]() {
        auto* dlg = TutorialDialog::codeHealth(this);
        dlg->exec();
        dlg->deleteLater();
    });

    auto* quickRef = learningMenu->addAction("Quick Reference");
    connect(quickRef, &QAction::triggered, this, [this]() {
        if (m_basicsOverlay) {
            m_basicsOverlay->goToPage(0);
            m_basicsOverlay->setVisible(true);
            m_basicsOverlay->raise();
        }
    });

    // ── Games section ─────────────────────────────────────────────────────
    learningMenu->addSeparator();

    auto* debugGame = learningMenu->addAction("Debug Game");
    connect(debugGame, &QAction::triggered, this, [this]() {
        if (!m_interactiveBasics) m_interactiveBasics = new InteractiveBasicsDialog(this);
        m_interactiveBasics->exec();
    });

    auto* predictGame = learningMenu->addAction("Predict Before You Run");
    connect(predictGame, &QAction::triggered, this, [this]() {
        if (m_buildBar) {
            // Toggle predict mode via the build bar's predict toggle signal
            emit m_buildBar->predictToggled(true);
        }
        if (m_predictPanel) {
            m_predictPanel->setActive(true);
        }
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
            dlg->setStyleSheet("QDialog { background: #1e1e2e; color: #cdd6f4; }");
            auto* lay = new QVBoxLayout(dlg);
            lay->setContentsMargins(16, 16, 16, 16);
            lay->setSpacing(8);

            auto* title = new QLabel("Pipeline Results");
            title->setStyleSheet("font-size: 15px; font-weight: bold; color: #cdd6f4;");
            lay->addWidget(title);

            auto* scroll = new QScrollArea(dlg);
            scroll->setWidgetResizable(true);
            scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");
            auto* inner = new QWidget;
            auto* innerLay = new QVBoxLayout(inner);
            innerLay->setSpacing(6);

            for (const auto& r : results) {
                auto* row = new QWidget;
                row->setStyleSheet("background: #2a2a3c; border-radius: 4px; padding: 6px;");
                auto* rowLay = new QVBoxLayout(row);
                rowLay->setContentsMargins(8, 6, 8, 6);
                rowLay->setSpacing(2);

                auto* header = new QLabel(QString("<b>%1</b> — %2")
                    .arg(r.checkName.toHtmlEscaped(),
                         r.passed ? "<span style='color:#a6e3a1;'>Passed</span>"
                                  : "<span style='color:#f38ba8;'>Issues found</span>"));
                header->setStyleSheet("color: #cdd6f4; font-size: 12px;");
                rowLay->addWidget(header);

                if (!r.summary.isEmpty()) {
                    auto* sumLbl = new QLabel(r.summary);
                    sumLbl->setWordWrap(true);
                    sumLbl->setStyleSheet("color: #a6adc8; font-size: 11px;");
                    rowLay->addWidget(sumLbl);
                }

                innerLay->addWidget(row);
            }

            innerLay->addStretch();
            scroll->setWidget(inner);
            lay->addWidget(scroll, 1);

            auto* closeBtn = new QPushButton("Close");
            closeBtn->setStyleSheet(
                "QPushButton { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
                " border-radius: 4px; padding: 6px 18px; font-size: 12px; }"
                "QPushButton:hover { background: #45475a; }");
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
