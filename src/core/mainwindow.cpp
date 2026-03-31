#include "core/mainwindow.h"
#include "core/workspace.h"
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
#include "core/costvisualizer.h"
#include "core/whatif.h"
#include "core/typeflow.h"
#include "core/machineview.h"
#include "core/theme.h"
#include "core/buildbar.h"
#include "core/buildsystem.h"
#include "core/runtimestrip.h"
#include "core/setupwizard.h"
#include "core/settingspanel.h"
#include "core/cvemonitor.h"
#include "editor/editor.h"
#include "editor/filetree.h"
#include "panels/clarity.h"
#include "panels/aichat.h"
#include "panels/langguide.h"
#include "panels/basics.h"
#include "core/interactivebasics.h"
#include "core/favoritesbar.h"
#include "core/buildchain.h"
#include "panels/expander.h"
#include "panels/levels.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#include <windowsx.h>
#endif

#include <QApplication>
#include <QSettings>
#include <QTimer>
#include <QFileInfo>
#include <QIcon>
#include <QDialog>
#include <QCloseEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QShowEvent>
#include <QWindow>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Code Clarity");
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
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

    // Build system — must be created before setupCentralLayout wires its signals
    m_buildSystem = new BuildSystem(this);

    createMenuBar();
    setupCentralLayout();
    createStatusBar();
    wireSignals();

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

    // CVE monitor — opt-in only, restored from saved user preference
    m_cveMonitor = new CVEMonitor(this);
    connect(m_cveMonitor, &CVEMonitor::vulnerabilitiesFound, this,
        [this](int count, const QStringList& packages) {
            showCVENotificationBar(count, packages);
        });
    // Restore last-saved workspace root and user settings
    if (m_fileTree) m_cveMonitor->setWorkspaceRoot(m_fileTree->rootPath());
    m_cveMonitor->setFrequencyMs(CVEMonitor::loadFrequencyMs());
    if (CVEMonitor::loadEnabled())
        m_cveMonitor->setEnabled(true);
    // Wire monitor into SecurityTestingFrame so its CVE tab can control it
    if (m_securityFrame)
        m_securityFrame->setCVEMonitor(m_cveMonitor);
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

// ── View Switching Helpers ────────────────────────────────────────────────
void MainWindow::hideAllAnalysisFrames()
{
    if (m_securityFrame)   m_securityFrame->setVisible(false);
    if (m_securityLab)     m_securityLab->setVisible(false);
    if (m_runtimeAnalysis) m_runtimeAnalysis->setVisible(false);
    if (m_memoryAnalysis)  m_memoryAnalysis->setVisible(false);
    if (m_depAnalysis)     m_depAnalysis->setVisible(false);
    if (m_codeHealth)      m_codeHealth->setVisible(false);
    if (m_debugFrame)      m_debugFrame->setVisible(false);
    if (m_dataTrace)       m_dataTrace->setVisible(false);
    if (m_errorJournal)    m_errorJournal->setVisible(false);
    if (m_typeFlow)        m_typeFlow->setVisible(false);
}

void MainWindow::showAnalysisFrame(QWidget* frame)
{
    m_mainSplitter->setVisible(false);
    hideAllAnalysisFrames();
    if (frame) frame->setVisible(true);
}

void MainWindow::returnToEditor()
{
    hideAllAnalysisFrames();
    m_mainSplitter->setVisible(true);
}

// Connects action->triggered() to showAnalysisFrame(frame), then runs backFn
// to wire the frame's own back signal to returnToEditor(). setupFn is called
// in the triggered handler before showAnalysisFrame, allowing per-frame init
// (e.g. setCode(), setTargetFile()).
void MainWindow::connectFrameToggle(QAction* action, QWidget* frame,
                                    std::function<void()> setupFn,
                                    std::function<void()> backFn)
{
    connect(action, &QAction::triggered, this, [this, frame, setupFn]() {
        if (setupFn) setupFn();
        showAnalysisFrame(frame);
    });
    if (backFn) backFn();
}

// ── Theme ────────────────────────────────────────────────────────────────
void MainWindow::applyTheme()
{
    qApp->setStyleSheet(Theme::themeStyleSheet(m_isDarkTheme));
    applyTitleBarTheme();
    updateDidYouKnowBanner();

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
    if (m_favoritesBar)
        m_favoritesBar->applyTheme(m_isDarkTheme);
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
    update();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_titleBar && event->type() == QEvent::MouseButtonDblClick) {
        if (isMaximized())
            showNormal();
        else
            showMaximized();
        return true;
    }

    if (watched == m_titleBar && event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && windowHandle()) {
            windowHandle()->startSystemMove();
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange && m_maximizeBtn) {
        m_maximizeBtn->setText(isMaximized()
            ? QString::fromUtf8("\xe2\x9d\x90")  // ❐
            : QString::fromUtf8("\xe2\x96\xa1")); // □
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    applyWin32Frameless();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);

    const int radius = 8;
    QRect r = rect();
    r.adjust(0, 0, -1, -1);
    QPainterPath path;
    path.addRoundedRect(r, radius, radius);
    setMask(QRegion(path.toFillPolygon().toPolygon()));
}

void MainWindow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int radius = 8;
    const int borderWidth = 2;
    QRect r = rect();
    r.adjust(borderWidth - 1, borderWidth - 1, -(borderWidth - 1), -(borderWidth - 1));

    const QColor bg = m_isDarkTheme ? QColor("#1e1e2e") : QColor("#f1f3f6");
    const QColor border = m_isDarkTheme ? QColor("#3c3c54") : QColor("#b8bec8");

    painter.setPen(QPen(border, borderWidth));
    painter.setBrush(bg);
    painter.drawRoundedRect(r, radius, radius);
}

void MainWindow::applyWin32Frameless()
{
#ifdef Q_OS_WIN
    if (!isVisible())
        return;

    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd)
        return;

    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);
    SetWindowLongPtr(hwnd, GWL_STYLE, style);

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
#endif
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    Q_UNUSED(eventType);
    MSG* msg = static_cast<MSG*>(message);

    switch (msg->message) {
    case WM_NCCALCSIZE:
        *result = 0;
        return true;
    case WM_NCHITTEST: {
        const LONG border = 8;
        RECT winRect;
        GetWindowRect(msg->hwnd, &winRect);

        const LONG x = GET_X_LPARAM(msg->lParam);
        const LONG y = GET_Y_LPARAM(msg->lParam);

        const bool resizeWidth = minimumWidth() != maximumWidth();
        const bool resizeHeight = minimumHeight() != maximumHeight();

        if (resizeWidth) {
            if (x >= winRect.left && x < winRect.left + border) {
                *result = HTLEFT;
                return true;
            }
            if (x < winRect.right && x >= winRect.right - border) {
                *result = HTRIGHT;
                return true;
            }
        }
        if (resizeHeight) {
            if (y >= winRect.top && y < winRect.top + border) {
                *result = HTTOP;
                return true;
            }
            if (y < winRect.bottom && y >= winRect.bottom - border) {
                *result = HTBOTTOM;
                return true;
            }
        }

        const QPoint localPos = mapFromGlobal(QPoint(x, y));
        if (m_titleBar && m_titleBar->rect().contains(localPos)) {
            QWidget* child = childAt(localPos);
            if (child == m_minimizeBtn || child == m_maximizeBtn || child == m_closeBtn) {
                *result = HTCLIENT;
                return true;
            }
            *result = HTCAPTION;
            return true;
        }
        break;
    }
    default:
        break;
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
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

// ── Favorites Bar Setup ──────────────────────────────────────────────────
void MainWindow::setupFavoritesBar()
{
    if (!m_favoritesBar) return;

    // Register all available tools with the favorites bar
    m_favoritesBar->registerTool("fullReport", "Full Report", [this]() {
        if (!m_codeHealth || !m_editor) return;
        m_codeHealth->setCode(m_editor->currentContent(), currentLangKey());
        showAnalysisFrame(m_codeHealth);
    });

    m_favoritesBar->registerTool("security", "Security", [this]() {
        if (!m_securityFrame || !m_editor) return;
        m_securityFrame->setCode(m_editor->currentContent(), currentLangKey(), m_editor->currentFilePath());
        m_mainSplitter->setVisible(false);
        m_securityFrame->setVisible(true);
    });

    m_favoritesBar->registerTool("runtime", "Runtime Analysis", [this]() {
        if (!m_runtimeAnalysis) return;
        m_mainSplitter->setVisible(false);
        if (m_securityFrame) m_securityFrame->setVisible(false);
        if (m_memoryAnalysis) m_memoryAnalysis->setVisible(false);
        if (m_depAnalysis) m_depAnalysis->setVisible(false);
        m_runtimeAnalysis->setVisible(true);
    });

    m_favoritesBar->registerTool("memcheck", "Mem Check", [this]() {
        if (!m_memoryAnalysis || !m_editor) return;
        m_memoryAnalysis->setFileInfo(m_editor->currentFilePath(), QString(), currentLangKey());
        m_mainSplitter->setVisible(false);
        if (m_securityFrame) m_securityFrame->setVisible(false);
        if (m_runtimeAnalysis) m_runtimeAnalysis->setVisible(false);
        if (m_depAnalysis) m_depAnalysis->setVisible(false);
        m_memoryAnalysis->setVisible(true);
    });

    m_favoritesBar->registerTool("depcheck", "Dep Check", [this]() {
        if (!m_depAnalysis || !m_editor) return;
        m_depAnalysis->setCode(m_editor->currentContent(), currentLangKey(), m_editor->currentFilePath());
        m_mainSplitter->setVisible(false);
        if (m_securityFrame) m_securityFrame->setVisible(false);
        if (m_runtimeAnalysis) m_runtimeAnalysis->setVisible(false);
        if (m_memoryAnalysis) m_memoryAnalysis->setVisible(false);
        m_depAnalysis->setVisible(true);
    });

    m_favoritesBar->registerTool("debugger", "Debugger", [this]() {
        if (!m_debugFrame || !m_editor) return;
        QString filePath = m_editor->currentFilePath();
        if (filePath.isEmpty()) {
            QMessageBox::information(this, "Save First", "Please save your file before debugging.");
            return;
        }
        m_editor->saveCurrentFile();
        m_debugFrame->setTargetFile(filePath, currentLangKey());
        m_mainSplitter->setVisible(false);
        if (m_securityFrame)   m_securityFrame->setVisible(false);
        if (m_runtimeAnalysis) m_runtimeAnalysis->setVisible(false);
        if (m_memoryAnalysis)  m_memoryAnalysis->setVisible(false);
        if (m_depAnalysis)     m_depAnalysis->setVisible(false);
        if (m_codeHealth)      m_codeHealth->setVisible(false);
        m_debugFrame->setVisible(true);
        m_debugFrame->startDebugging();
    });

    m_favoritesBar->registerTool("typeflow", "Type Flow", [this]() {
        if (!m_typeFlow || !m_editor) return;
        m_typeFlow->setCode(m_editor->currentContent(), currentLangKey());
        m_mainSplitter->setVisible(false);
        if (m_securityFrame)   m_securityFrame->setVisible(false);
        if (m_runtimeAnalysis) m_runtimeAnalysis->setVisible(false);
        if (m_memoryAnalysis)  m_memoryAnalysis->setVisible(false);
        if (m_depAnalysis)     m_depAnalysis->setVisible(false);
        if (m_codeHealth)      m_codeHealth->setVisible(false);
        if (m_debugFrame)      m_debugFrame->setVisible(false);
        if (m_dataTrace)       m_dataTrace->setVisible(false);
        if (m_errorJournal)    m_errorJournal->setVisible(false);
        m_typeFlow->setVisible(true);
    });

    m_favoritesBar->registerTool("errorjournal", "Error Journal", [this]() {
        if (!m_errorJournal) return;
        if (m_fileTree && !m_fileTree->rootPath().isEmpty())
            m_errorJournal->setWorkspaceRoot(m_fileTree->rootPath());
        m_mainSplitter->setVisible(false);
        if (m_securityFrame)   m_securityFrame->setVisible(false);
        if (m_runtimeAnalysis) m_runtimeAnalysis->setVisible(false);
        if (m_memoryAnalysis)  m_memoryAnalysis->setVisible(false);
        if (m_depAnalysis)     m_depAnalysis->setVisible(false);
        if (m_codeHealth)      m_codeHealth->setVisible(false);
        if (m_debugFrame)      m_debugFrame->setVisible(false);
        if (m_dataTrace)       m_dataTrace->setVisible(false);
        m_errorJournal->setVisible(true);
    });

    m_favoritesBar->registerTool("tracevar", "Trace Var", [this]() {
        if (!m_dataTrace || !m_editor) return;
        QString selected = m_editor->codeEditor()->textCursor().selectedText().trimmed();
        m_dataTrace->setCode(m_editor->currentContent(), currentLangKey(), m_editor->currentFilePath());
        if (!selected.isEmpty()) m_dataTrace->setVariable(selected);
        m_mainSplitter->setVisible(false);
        if (m_securityFrame)   m_securityFrame->setVisible(false);
        if (m_runtimeAnalysis) m_runtimeAnalysis->setVisible(false);
        if (m_memoryAnalysis)  m_memoryAnalysis->setVisible(false);
        if (m_depAnalysis)     m_depAnalysis->setVisible(false);
        if (m_codeHealth)      m_codeHealth->setVisible(false);
        if (m_debugFrame)      m_debugFrame->setVisible(false);
        m_dataTrace->setVisible(true);
    });

    m_favoritesBar->reload();
}

// ── Run Build Chain ───────────────────────────────────────────────────────
void MainWindow::runBuildChain()
{
    if (!m_editor) return;
    QString filePath = m_editor->currentFilePath();
    if (filePath.isEmpty()) {
        QMessageBox::information(this, "Save First",
            "Please save your file before running a build chain.");
        return;
    }
    m_editor->saveCurrentFile();

    QString chainName = m_buildBar ? m_buildBar->currentChainName() : "Default";
    QStringList savedNames = BuildChainConfig::savedChainNames();
    BuildChainConfig cfg = savedNames.contains(chainName)
        ? BuildChainConfig::load(chainName)
        : BuildChainConfig::defaultConfig();

    m_buildBar->clearResults();
    m_buildBar->showResults();
    m_buildBar->showOutput(QString("<b>Build Chain: %1</b><br>").arg(cfg.name.toHtmlEscaped()));

    auto* runner = new BuildChainRunner(cfg, this);

    connect(runner, &BuildChainRunner::stepStarted, this,
        [this](const QString&, const QString& label) {
            if (m_buildBar)
                m_buildBar->showOutput(QString("<span style='color:#89b4fa;'>Running: %1...</span><br>")
                    .arg(label.toHtmlEscaped()));
        });

    connect(runner, &BuildChainRunner::stepFinished, this,
        [this](const QString&, bool passed, const QString& output) {
            QString color = passed ? "#a6e3a1" : "#f38ba8";
            QString status = passed ? "PASS" : "FAIL";
            if (m_buildBar) {
                m_buildBar->showOutput(
                    QString("<span style='color:%1;'>[%2]</span> %3<br>")
                        .arg(color, status, output.toHtmlEscaped().replace("\n", "<br>")));
            }
        });

    connect(runner, &BuildChainRunner::chainFinished, this,
        [this, runner](bool allPassed) {
            if (m_buildBar) {
                QString color = allPassed ? "#a6e3a1" : "#f38ba8";
                QString msg   = allPassed ? "Build chain completed successfully." : "Build chain stopped — step failed.";
                m_buildBar->showOutput(QString("<br><b style='color:%1;'>%2</b>").arg(color, msg));
            }
            runner->deleteLater();
        });

    runner->run(filePath, currentLangKey());
}

// ── CVE Notification Bar ──────────────────────────────────────────────────
void MainWindow::showCVENotificationBar(int count, const QStringList& packages)
{
    if (!m_cveBar) return;

    QString msg = QString("  %1 dependenc%2 %3 known vulnerabilities.  ")
        .arg(count)
        .arg(count == 1 ? "y has" : "ies have")
        .arg(count == 1 ? "a" : "");
    m_cveBarLabel->setText(msg + " [View Details]");
    m_cveBarPackages = packages;
    m_cveBar->setVisible(true);
}

// ── WorkspaceManager  (merged from workspace.cpp) ─────────────────────────

WorkspaceManager::WorkspaceManager(QObject *parent)
    : QObject(parent)
    , m_settings("CodeClarity", "CodeClarity")
{
}

QString WorkspaceManager::savedWorkspacePath() const
{
    return m_settings.value("workspace/path").toString();
}

void WorkspaceManager::setWorkspacePath(const QString &path)
{
    m_settings.setValue("workspace/path", path);
    emit workspaceChanged(path);
}

void WorkspaceManager::saveOpenTabs(const QStringList &paths)
{
    m_settings.setValue("workspace/openTabs", paths);
}

QStringList WorkspaceManager::savedOpenTabs() const
{
    return m_settings.value("workspace/openTabs").toStringList();
}

void WorkspaceManager::saveActiveTabIndex(int index)
{
    m_settings.setValue("workspace/activeTab", index);
}

int WorkspaceManager::savedActiveTabIndex() const
{
    return m_settings.value("workspace/activeTab", 0).toInt();
}

void WorkspaceManager::clearWorkspace()
{
    m_settings.remove("workspace/path");
    m_settings.remove("workspace/openTabs");
    m_settings.remove("workspace/activeTab");
}
