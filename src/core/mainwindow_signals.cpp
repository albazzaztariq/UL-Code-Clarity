#include "core/mainwindow.h"
#include "core/securitytesting.h"
#include "core/securitylab.h"
#include "core/runtimeanalysis.h"
#include "core/memoryanalysis.h"
#include "core/dependencyanalysis.h"
#include "core/codehealth.h"
#include "core/debugger.h"
#include "core/datatrace.h"
#include "core/predictpanel.h"
#include "core/errorjournal.h"
#include "core/typeflow.h"
#include "core/machineview.h"
#include "core/buildbar.h"
#include "core/buildsystem.h"
#include "core/aipermissions.h"
#include "editor/editor.h"
#include "editor/filetree.h"
#include "panels/clarity.h"
#include "panels/aichat.h"
#include "panels/levels.h"
#include "panels/langguide.h"
#include "panels/basics.h"
#include "panels/expander.h"
#include "core/whatif.h"

#include <QMenu>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextEdit>
#include <QFileInfo>
#include <QMessageBox>
#include <QTemporaryFile>
#include <QDir>
#include <QProcess>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QVBoxLayout>

// ── Wire All Signals ────────────────────────────────────────────────────
void MainWindow::wireSignals()
{
    // Level selector -> clarity panel verbosity + AI assist level
    connect(m_levelSelector, &LevelSelector::levelChanged, this, [this](int level) {
        m_clarityPanel->setLevel(level);
        if (m_aiChatPanel) m_aiChatPanel->setAssistLevel(level);
        if (m_runtimeAnalysis) m_runtimeAnalysis->setAssistLevel(level);
        buildToolsMenu();  // Rebuild tools menu with level-appropriate items
    });
    if (m_runtimeAnalysis && m_levelSelector)
        m_runtimeAnalysis->setAssistLevel(m_levelSelector->currentLevel());

    connect(m_levelSelector, &LevelSelector::showBuildButton, this, [this](bool show) {
        Q_UNUSED(show);
    });

    connect(m_levelSelector, &LevelSelector::showFileTree, this, [this](bool show) {
        m_fileTree->setVisible(show);
    });

    connect(m_levelSelector, &LevelSelector::showBasicsButton, this, [this](bool show) {
        Q_UNUSED(show);
    });

    // Clarity panel close -> hide clarity column
    connect(m_clarityPanel, &ClarityPanel::closeRequested, this, [this]() {
        if (m_clarityPanel) m_clarityPanel->toggleCollapsed();
    });

    // AI Chat panel close -> hide chat column
    connect(m_aiChatPanel, &AIChatPanel::closeRequested, this, [this]() {
        if (m_aiChatPanel) m_aiChatPanel->toggleCollapsed();
    });

    // Overlay close signals
    connect(m_langGuide,       &LangGuideOverlay::closeRequested,  m_langGuide,       &QWidget::hide);
    connect(m_basicsOverlay,   &BasicsOverlay::closeRequested,     m_basicsOverlay,   &QWidget::hide);
    connect(m_expanderOverlay, &ExpanderOverlay::closeRequested,   m_expanderOverlay, &QWidget::hide);

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
            m_aiChatPanel->sendAIMessage(text);
        }
    });

    // Walk through handler
    connect(m_walkHandler, &WalkThroughHandler::walkthroughGenerated, this,
        [this](const QString& explanation) {
            m_aiChatPanel->addMessage(ChatBubble::AI, explanation);
        });

    // Editor: Explain button
    connect(m_editor, &EditorWidget::explainRequested, this, [this]() {
        if (!m_editor) return;
        QString code = m_editor->codeEditor()->toPlainText();
        if (code.trimmed().isEmpty()) return;

        m_preExplainCode = code;
        m_explainActive = true;

        QString lang  = m_editor->currentLanguage();
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

    // Editor: Clear Explanations button
    connect(m_editor, &EditorWidget::clearExplainRequested, this, [this]() {
        if (!m_editor || !m_explainActive) return;
        m_editor->codeEditor()->setPlainText(m_preExplainCode);
        m_preExplainCode.clear();
        m_explainActive = false;
        m_editor->setExplainActive(false);
    });

    // Editor: Walk Me Through button
    connect(m_editor, &EditorWidget::walkThroughRequested, this, [this]() {
        if (!m_editor || !m_walkHandler) return;
        QString filePath = m_editor->currentFilePath();
        QFileInfo fi(filePath);
        QString filename = fi.fileName().isEmpty() ? "untitled" : fi.fileName();
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

    // Editor right-click: "What If...?" -> WhatIfDialog
    connect(m_editor->codeEditor(), &CodeEditor::whatIfRequested, this,
        [this](int lineNumber, const QString& lineText) {
            if (!m_editor) return;
            QString filePath = m_editor->currentFilePath();
            if (filePath.isEmpty()) {
                QMessageBox::information(this, "Save First",
                    "Please save your file before using What If.");
                return;
            }
            m_editor->saveCurrentFile();
            auto* dlg = new WhatIfDialog(filePath, lineNumber, lineText,
                                          currentLangKey(), this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->setWindowModality(Qt::NonModal);
            dlg->show();
        });

    // Runtime Analysis Frame signals
    connect(m_runtimeAnalysis, &RuntimeAnalysisFrame::backToEditor,
            this, &MainWindow::returnToEditor);
    connect(m_runtimeAnalysis, &RuntimeAnalysisFrame::compilationError, this,
        [this](const QString& filePath, const QString& errorText) {
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
    connect(m_securityFrame, &SecurityTestingFrame::backToEditor,
            this, &MainWindow::returnToEditor);
    connect(m_securityFrame, &SecurityTestingFrame::jumpToLine, this, [this](const QString& /*file*/, int lineNumber) {
        if (!m_editor) return;
        m_securityFrame->setVisible(false);
        m_mainSplitter->setVisible(true);
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
    connect(m_securityFrame, &SecurityTestingFrame::openLab, this,
        [this](const QString& vulnType) {
            if (m_securityLab) {
                m_securityLab->setVisible(false);
                m_securityLab->deleteLater();
                m_securityLab = nullptr;
            }
            m_securityLab = SecurityLabWidget::forVulnType(vulnType, centralWidget());
            if (!m_securityLab) return;
            auto* cl = qobject_cast<QVBoxLayout*>(centralWidget()->layout());
            if (cl) cl->insertWidget(cl->count() - 1, m_securityLab);
            connect(m_securityLab, &SecurityLabWidget::backToEditor, this, [this]() {
                if (m_securityLab) m_securityLab->setVisible(false);
                m_securityFrame->setVisible(true);
            });
            m_mainSplitter->setVisible(false);
            m_securityFrame->setVisible(false);
            if (m_runtimeAnalysis) m_runtimeAnalysis->setVisible(false);
            if (m_memoryAnalysis) m_memoryAnalysis->setVisible(false);
            if (m_depAnalysis) m_depAnalysis->setVisible(false);
            m_securityLab->setVisible(true);
        });

    // Memory Analysis Frame signals
    connect(m_memoryAnalysis, &MemoryAnalysisFrame::backToEditor,
            this, &MainWindow::returnToEditor);

    // Dependency Analysis Frame signals
    connect(m_depAnalysis, &DependencyAnalysisFrame::backToEditor,
            this, &MainWindow::returnToEditor);

    // Code Health Frame signals
    connect(m_codeHealth, &CodeHealthFrame::backToEditor,
            this, &MainWindow::returnToEditor);

    // Debug Frame signals
    connect(m_debugFrame, &DebugFrame::backToEditor, this, [this]() {
        m_debugFrame->stopDebugging();
        m_debugFrame->setVisible(false);
        m_mainSplitter->setVisible(true);
    });

    // DataTrace Frame signals
    connect(m_dataTrace, &DataTraceFrame::backToEditor,
            this, &MainWindow::returnToEditor);
    connect(m_dataTrace, &DataTraceFrame::jumpToLine, this, [this](const QString& /*file*/, int line) {
        m_dataTrace->setVisible(false);
        m_mainSplitter->setVisible(true);
        if (m_editor && line > 0) {
            auto* ed = m_editor->codeEditor();
            QTextCursor cursor = ed->textCursor();
            cursor.movePosition(QTextCursor::Start);
            cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, line - 1);
            ed->setTextCursor(cursor);
            ed->centerCursor();
        }
    });

    // ErrorJournal signals
    connect(m_errorJournal, &ErrorJournal::backToEditor,
            this, &MainWindow::returnToEditor);
    connect(m_errorJournal, &ErrorJournal::jumpToFile, this,
        [this](const QString& filePath, int line) {
            if (m_editor) {
                m_editor->openFile(filePath);
                m_errorJournal->setVisible(false);
                m_mainSplitter->setVisible(true);
                if (line > 0) {
                    auto* ed = m_editor->codeEditor();
                    QTextCursor cursor = ed->textCursor();
                    cursor.movePosition(QTextCursor::Start);
                    cursor.movePosition(QTextCursor::NextBlock,
                                        QTextCursor::MoveAnchor, line - 1);
                    ed->setTextCursor(cursor);
                    ed->centerCursor();
                }
            }
        });

    // PredictPanel: runRequested
    connect(m_predictPanel, &PredictPanel::runRequested, this, [this]() {
        if (!m_editor) return;
        QString filePath = m_editor->currentFilePath();
        if (filePath.isEmpty()) {
            QTemporaryFile* tmp = new QTemporaryFile(this);
            QString lk = currentLangKey();
            QString ext = (lk == "python") ? ".py" : (lk == "c") ? ".c" : ".cpp";
            tmp->setFileTemplate(QDir::tempPath() + "/ccpred_XXXXXX" + ext);
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

    // BuildSystem finished — feed output to PredictPanel + ErrorJournal
    connect(m_buildSystem, &BuildSystem::finished, this,
        [this](bool success) {
            QTextEdit* te = m_buildBar ? m_buildBar->resultsContent() : nullptr;
            QString actualOutput = te ? te->toPlainText() : QString();

            if (m_predictPanel && m_predictPanel->isActive() && m_predictPanel->isVisible()) {
                m_predictPanel->checkPrediction(actualOutput);
            }

            QString filePath = m_editor ? m_editor->currentFilePath() : QString();
            if (m_errorJournal) {
                if (!success && !actualOutput.isEmpty()) {
                    m_errorJournal->logError(actualOutput, filePath);
                } else if (success && !filePath.isEmpty()) {
                    m_errorJournal->logSuccess(filePath);
                }
                if (m_fileTree && !m_fileTree->rootPath().isEmpty())
                    m_errorJournal->setWorkspaceRoot(m_fileTree->rootPath());
            }
        });

    // Build bar debug button
    connect(m_buildBar, &BuildBar::debugRequested, this, [this]() {
        if (!m_editor) return;
        QString filePath = m_editor->currentFilePath();
        if (filePath.isEmpty()) {
            QMessageBox::information(this, "Save First",
                "Please save your file before debugging.");
            return;
        }
        m_editor->saveCurrentFile();
        m_debugFrame->setTargetFile(filePath, currentLangKey());
        showAnalysisFrame(m_debugFrame);
        m_debugFrame->startDebugging();
    });

    // Build bar: Predict toggle
    connect(m_buildBar, &BuildBar::predictToggled, this, [this](bool on) {
        if (m_predictPanel) {
            m_predictPanel->setActive(on);
            if (on) m_predictPanel->reset();
        }
    });

    // Build bar: Error Journal button
    connect(m_buildBar, &BuildBar::errorJournalRequested, this, [this]() {
        if (!m_errorJournal) return;
        if (m_fileTree && !m_fileTree->rootPath().isEmpty())
            m_errorJournal->setWorkspaceRoot(m_fileTree->rootPath());
        showAnalysisFrame(m_errorJournal);
    });

    // Error badge sync
    connect(m_errorJournal, &ErrorJournal::errorCountChanged, this,
        [this](int count) {
            if (m_buildBar) m_buildBar->setErrorBadge(count);
        });

    // Context menu on editor for "Trace This Variable"
    m_editor->codeEditor()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_editor->codeEditor(), &QWidget::customContextMenuRequested, this,
        [this](const QPoint& pos) {
            QMenu* menu = m_editor->codeEditor()->createStandardContextMenu();
            menu->addSeparator();
            auto* traceAction = menu->addAction("Trace This Variable");
            QString selected = m_editor->codeEditor()->textCursor().selectedText().trimmed();
            traceAction->setEnabled(!selected.isEmpty());
            connect(traceAction, &QAction::triggered, this, [this, selected]() {
                if (!m_dataTrace || !m_editor) return;
                QString code     = m_editor->currentContent();
                QString lang     = currentLangKey();
                QString filePath = m_editor->currentFilePath();
                m_dataTrace->setCode(code, lang, filePath);
                m_dataTrace->setVariable(selected);
                showAnalysisFrame(m_dataTrace);
            });
            menu->exec(m_editor->codeEditor()->mapToGlobal(pos));
            menu->deleteLater();
        });

    // TypeFlow Frame signals
    connect(m_typeFlow, &TypeFlowFrame::backToEditor,
            this, &MainWindow::returnToEditor);
    connect(m_typeFlow, &TypeFlowFrame::jumpToLine, this, [this](const QString& /*file*/, int line) {
        if (!m_editor) return;
        QTextBlock block = m_editor->codeEditor()->document()->findBlockByNumber(line - 1);
        if (block.isValid()) {
            QTextCursor cur(block);
            m_editor->codeEditor()->setTextCursor(cur);
            m_editor->codeEditor()->centerCursor();
        }
    });

    // Machine View Panel signals
    connect(m_machineView, &MachineViewPanel::closeRequested, this, [this]() {
        m_machineView->setVisible(false);
    });
    connect(m_editor->codeEditor(), &QPlainTextEdit::cursorPositionChanged, this, [this]() {
        if (!m_machineView || !m_machineView->isVisible()) return;
        QTextCursor cur = m_editor->codeEditor()->textCursor();
        int lineNum = cur.blockNumber() + 1;
        QString lineText = cur.block().text();
        if (m_levelSelector) m_machineView->setLevel(m_levelSelector->currentLevel());
        m_machineView->setLanguage(currentLangKey());
        m_machineView->explainLine(lineNum, lineText);
    });
    connect(m_levelSelector, &LevelSelector::levelChanged, this, [this](int level) {
        if (m_machineView) m_machineView->setLevel(level);
    });

    // AI Chat: keep editor context in sync
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

    // AI Chat: code block received -> apply to editor + log to Clarity
    connect(m_aiChatPanel, &AIChatPanel::codeBlockReceived, this,
        [this](const QString& code, const QString& lang) {
            if (m_editor) {
                QString filePath = m_editor->currentFilePath();
                QString wsRoot   = m_fileTree ? m_fileTree->rootPath() : QString();

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
