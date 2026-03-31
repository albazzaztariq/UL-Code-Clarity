#include "core/mainwindow.h"
#include "core/lspmanager.h"
#include "core/newsticker.h"
#include "core/favoritesbar.h"
#include "core/securitytesting.h"
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
#include "core/settingspanel.h"
#include "core/buildbar.h"
#include "core/buildsystem.h"
#include "core/runtimestrip.h"
#include "core/jsonloader.h"
#include "editor/editor.h"
#include "editor/filetree.h"
#include "panels/clarity.h"
#include "panels/aichat.h"
#include "panels/langguide.h"
#include "panels/basics.h"
#include "panels/expander.h"
#include "panels/levels.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QAction>
#include <QStatusBar>
#include <QFileDialog>
#include <QApplication>
#include <QSettings>
#include <QPushButton>
#include <QFrame>
#include <QTextEdit>
#include <QMessageBox>
#include <QTemporaryFile>
#include <QDir>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QMenuBar>
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <QRandomGenerator>
#include <QTextDocument>
#include <QTimer>
#include <QRegularExpression>

namespace {
QPixmap tintPixmap(const QPixmap& src, const QColor& color)
{
    if (src.isNull())
        return src;

    QPixmap tinted(src.size());
    tinted.fill(Qt::transparent);

    QPainter painter(&tinted);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawPixmap(0, 0, src);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(tinted.rect(), color);
    painter.end();

    return tinted;
}
} // namespace

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

    // Spacer
    m_statusBarSpacer = new QWidget;
    m_statusBarSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_statusBarSpacer->setStyleSheet("background: #181825;");
    sb->addWidget(m_statusBarSpacer);

    // Level selector in status bar
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
        connect(&dlg, &SettingsPanel::menuVisibilityChanged, this, [this]() {
            applyToolsMenuVisibility();
        });
        connect(&dlg, &SettingsPanel::tickerSettingsChanged, this, [this]() {
            if (m_newsTicker) m_newsTicker->applySettings();
        });
        connect(&dlg, &SettingsPanel::cveSettingsChanged, this, [this]() {
            if (m_cveMonitor) {
                m_cveMonitor->setFrequencyMs(CVEMonitor::loadFrequencyMs());
                m_cveMonitor->setEnabled(CVEMonitor::loadEnabled());
            }
            if (m_securityFrame) m_securityFrame->reloadCVESettings();
        });
        dlg.exec();
        if (m_aiChatPanel) m_aiChatPanel->reloadModelFromConfig();
        updateDidYouKnowBanner();
        if (m_buildBar) m_buildBar->updatePredictVisibility();
    });
    sb->addPermanentWidget(settingsGearBtn);

    // Theme toggle button
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

void MainWindow::applyTitleBarTheme()
{
    if (!m_titleBar)
        return;

    if (m_isDarkTheme) {
        m_titleBar->setStyleSheet(
            "QWidget#TitleBar { background: #1e1e2e; border-bottom: 1px solid #313244; }");
        m_windowTitleLabel->setStyleSheet(
            "QLabel { color: #cdd6f4; font-size: 12px; font-weight: 600; }");
        m_minimizeBtn->setStyleSheet(
            "QPushButton { background: transparent; color: #bac2de; font-size: 14px; border: none; }"
            "QPushButton:hover { background: #313244; color: #ffffff; }");
        m_maximizeBtn->setStyleSheet(
            "QPushButton { background: transparent; color: #bac2de; font-size: 14px; border: none; }"
            "QPushButton:hover { background: #313244; color: #ffffff; }");
        m_closeBtn->setStyleSheet(
            "QPushButton { background: transparent; color: #bac2de; font-size: 14px; border: none; }"
            "QPushButton:hover { background: #f38ba8; color: #1e1e2e; }");
    } else {
        m_titleBar->setStyleSheet(
            "QWidget#TitleBar { background: #e1e4ea; border-bottom: 1px solid #c9ced8; }");
        m_windowTitleLabel->setStyleSheet(
            "QLabel { color: #2f343b; font-size: 12px; font-weight: 600; }");
        m_minimizeBtn->setStyleSheet(
            "QPushButton { background: transparent; color: #4b515a; font-size: 14px; border: none; }"
            "QPushButton:hover { background: #d6dbe3; color: #2f343b; }");
        m_maximizeBtn->setStyleSheet(
            "QPushButton { background: transparent; color: #4b515a; font-size: 14px; border: none; }"
            "QPushButton:hover { background: #d6dbe3; color: #2f343b; }");
        m_closeBtn->setStyleSheet(
            "QPushButton { background: transparent; color: #4b515a; font-size: 14px; border: none; }"
            "QPushButton:hover { background: #d86a76; color: #ffffff; }");
    }
}

void MainWindow::setupDidYouKnowBanner()
{
    if (m_didYouKnowBanner)
        return;

    m_didYouKnowBanner = new QWidget;
    m_didYouKnowBanner->setObjectName("DidYouKnowBanner");
    m_didYouKnowBanner->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* layout = new QHBoxLayout(m_didYouKnowBanner);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(8);

    m_didYouKnowLabel = new QLabel;
    m_didYouKnowLabel->setWordWrap(true);
    m_didYouKnowLabel->setTextFormat(Qt::RichText);
    layout->addWidget(m_didYouKnowLabel, 1);

    updateDidYouKnowBanner();

    if (!m_didYouKnowTimer) {
        m_didYouKnowTimer = new QTimer(this);
        m_didYouKnowTimer->setInterval(30000);
        connect(m_didYouKnowTimer, &QTimer::timeout,
                this, &MainWindow::updateDidYouKnowBanner);
        m_didYouKnowTimer->start();
    }
}

void MainWindow::updateDidYouKnowBanner()
{
    if (!m_didYouKnowBanner || !m_didYouKnowLabel)
        return;

    QSettings s("CodeClarity", "CodeClarity");
    const bool enabled = s.value("ui/didYouKnowEnabled", true).toBool();
    m_didYouKnowBanner->setVisible(enabled);
    if (!enabled) {
        if (m_didYouKnowTimer) m_didYouKnowTimer->stop();
        return;
    }
    if (m_didYouKnowTimer && !m_didYouKnowTimer->isActive())
        m_didYouKnowTimer->start();

    m_didYouKnowLabel->setText(buildDidYouKnowHtml(m_isDarkTheme));

    if (m_isDarkTheme) {
        m_didYouKnowBanner->setStyleSheet(
            "QWidget#DidYouKnowBanner { background: #11111b; border-bottom: 1px solid #313244; }");
        m_didYouKnowLabel->setStyleSheet("QLabel { font-size: 12px; }");
    } else {
        m_didYouKnowBanner->setStyleSheet(
            "QWidget#DidYouKnowBanner { background: #e9ecf1; border-bottom: 1px solid #cfd4dd; }");
        m_didYouKnowLabel->setStyleSheet("QLabel { font-size: 12px; }");
    }
}

QString MainWindow::buildDidYouKnowHtml(bool isDark)
{
    QJsonObject root = JsonLoader::loadObject("tutorials.json");
    QList<QPair<QString, QString>> entries;

    for (auto it = root.begin(); it != root.end(); ++it) {
        if (!it->isArray())
            continue;
        const QJsonArray arr = it->toArray();
        for (const QJsonValue& val : arr) {
            if (!val.isObject())
                continue;
            const QJsonObject obj = val.toObject();
            const QString title = obj.value("title").toString();
            const QString html = obj.value("html").toString();
            if (!title.isEmpty() && !html.isEmpty())
                entries.append({title, html});
        }
    }

    QString title = "Quick Tip";
    QString body = "Build and run a small sample to verify your pipeline.";

    const int bannerWidth = m_didYouKnowBanner ? m_didYouKnowBanner->width() : 900;
    const QFontMetrics fm(m_didYouKnowLabel ? m_didYouKnowLabel->font() : QFont());
    const int avgWidth = qMax(6, fm.averageCharWidth());
    const int maxChars = qMax(160, (bannerWidth / avgWidth) * 2);

    auto assembleSentences = [&](const QString& text) {
        QStringList sentences = text.split(QRegularExpression("(?<=[.!?])\\s+"),
                                           Qt::SkipEmptyParts);
        QString assembled;
        for (const QString& s : sentences) {
            QString next = assembled.isEmpty() ? s : (assembled + " " + s);
            if (next.size() <= maxChars)
                assembled = next;
            else
                break;
        }
        return assembled;
    };

    if (!entries.isEmpty()) {
        int start = QRandomGenerator::global()->bounded(entries.size());
        for (int i = 0; i < entries.size(); ++i) {
            int idx = (start + i) % entries.size();
            if (idx == m_lastDidYouKnowIndex && entries.size() > 1)
                continue;
            QTextDocument doc;
            doc.setHtml(entries[idx].second);
            QString plain = doc.toPlainText().simplified();
            QString candidate = assembleSentences(plain);
            if (candidate.isEmpty())
                continue;
            title = entries[idx].first;
            body = candidate;
            m_lastDidYouKnowIndex = idx;
            break;
        }
    }

    const QString bodyColor = isDark ? "#cdd6f4" : "#333333";
    return QString(
        "<span style='color:#38bdf8; font-weight:700;'>Did You Know?</span> "
        "<span style='color:%1;'><b>%2</b> — %3</span>")
        .arg(bodyColor, title.toHtmlEscaped(), body.toHtmlEscaped());
}

// ── Custom Title Bar ─────────────────────────────────────────────────────
void MainWindow::setupTitleBar()
{
    if (m_titleBar)
        return;

    m_titleBar = new QWidget;
    m_titleBar->setFixedHeight(38);
    m_titleBar->setObjectName("TitleBar");
    m_titleBar->installEventFilter(this);

    auto* titleLayout = new QHBoxLayout(m_titleBar);
    titleLayout->setContentsMargins(10, 0, 0, 0);
    titleLayout->setSpacing(8);
    titleLayout->setAlignment(Qt::AlignVCenter);

    m_windowIconLabel = new QLabel;
    m_windowIconLabel->setFixedSize(20, 20);
    m_windowIconLabel->setPixmap(tintPixmap(windowIcon().pixmap(16, 16), QColor("#38bdf8")));
    m_windowIconLabel->setAlignment(Qt::AlignCenter);
    m_windowIconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    titleLayout->addWidget(m_windowIconLabel);

    m_windowTitleLabel = new QLabel("Code Clarity");
    m_windowTitleLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_windowTitleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    titleLayout->addWidget(m_windowTitleLabel);

    titleLayout->addStretch();

    m_minimizeBtn = new QPushButton(QString::fromUtf8("\xe2\x80\x94")); // —
    m_maximizeBtn = new QPushButton(QString::fromUtf8("\xe2\x96\xa1")); // □
    m_closeBtn = new QPushButton(QString::fromUtf8("\xe2\x9c\x95"));    // ✕

    const QSize btnSize(46, 38);
    m_minimizeBtn->setFixedSize(btnSize);
    m_maximizeBtn->setFixedSize(btnSize);
    m_closeBtn->setFixedSize(btnSize);

    titleLayout->addWidget(m_minimizeBtn);
    titleLayout->addWidget(m_maximizeBtn);
    titleLayout->addWidget(m_closeBtn);

    connect(m_minimizeBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(m_maximizeBtn, &QPushButton::clicked, this, [this]() {
        if (isMaximized())
            showNormal();
        else
            showMaximized();
    });
    connect(m_closeBtn, &QPushButton::clicked, this, &QWidget::close);

    if (menuBar()) {
        menuBar()->setNativeMenuBar(false);
        auto* menuContainer = new QWidget;
        auto* menuLayout = new QVBoxLayout(menuContainer);
        menuLayout->setContentsMargins(2, 2, 2, 0);
        menuLayout->setSpacing(0);
        menuLayout->addWidget(m_titleBar);
        menuLayout->addWidget(menuBar());
        setMenuWidget(menuContainer);
    }

    applyTitleBarTheme();
}

// ── Central Layout ──────────────────────────────────────────────────────
void MainWindow::setupCentralLayout()
{
    setupTitleBar();

    auto* centralWidget = new QWidget;
    auto* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(2, 0, 2, 2);
    mainLayout->setSpacing(0);

    // News ticker — sits directly below menu bar
    m_newsTicker = new NewsTicker;
    mainLayout->addWidget(m_newsTicker);

    // Did You Know banner — fills the gap below the ticker
    setupDidYouKnowBanner();
    mainLayout->addWidget(m_didYouKnowBanner);

    // Favorites bar — customizable tool buttons strip
    m_favoritesBar = new FavoritesBar;
    mainLayout->addWidget(m_favoritesBar);

    // CVE notification bar — yellow, hidden by default
    m_cveBar = new QWidget;
    m_cveBar->setStyleSheet("background: #f9e2af; border-bottom: 1px solid #e6a817;");
    m_cveBar->setFixedHeight(28);
    m_cveBar->setVisible(false);
    {
        auto* barLay = new QHBoxLayout(m_cveBar);
        barLay->setContentsMargins(10, 0, 10, 0);
        barLay->setSpacing(6);

        m_cveBarLabel = new QLabel;
        m_cveBarLabel->setStyleSheet("color: #7c5e00; font-size: 11px; font-weight: 600;");
        barLay->addWidget(m_cveBarLabel);

        barLay->addStretch();

        auto* viewBtn = new QPushButton("View Details");
        viewBtn->setFlat(true);
        viewBtn->setCursor(Qt::PointingHandCursor);
        viewBtn->setStyleSheet(
            "QPushButton { color: #7c5e00; font-size: 11px; font-weight: 600;"
            " text-decoration: underline; background: none; border: none; padding: 0; }"
            "QPushButton:hover { color: #4a3800; }");
        connect(viewBtn, &QPushButton::clicked, this, [this]() {
            if (!m_depAnalysis) return;
            QString code, lang, filePath;
            if (m_editor) {
                code     = m_editor->currentContent();
                lang     = currentLangKey();
                filePath = m_editor->currentFilePath();
            }
            m_depAnalysis->setCode(code, lang, filePath);
            m_mainSplitter->setVisible(false);
            if (m_securityFrame)   m_securityFrame->setVisible(false);
            if (m_runtimeAnalysis) m_runtimeAnalysis->setVisible(false);
            if (m_memoryAnalysis)  m_memoryAnalysis->setVisible(false);
            m_depAnalysis->setVisible(true);
        });
        barLay->addWidget(viewBtn);

        auto* dismissBtn = new QPushButton("x");
        dismissBtn->setFlat(true);
        dismissBtn->setFixedSize(16, 16);
        dismissBtn->setCursor(Qt::PointingHandCursor);
        dismissBtn->setStyleSheet(
            "QPushButton { color: #7c5e00; font-size: 12px; background: none; border: none; padding: 0; }"
            "QPushButton:hover { color: #4a3800; }");
        connect(dismissBtn, &QPushButton::clicked, m_cveBar, &QWidget::hide);
        barLay->addWidget(dismissBtn);
    }
    mainLayout->addWidget(m_cveBar);

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

    // Center: Editor
    m_editor = new EditorWidget;
    m_mainSplitter->addWidget(m_editor);

    // Right: Two side-by-side columns (Clarity + AI Chat)
    m_rightSplitter = new QSplitter(Qt::Horizontal);
    m_rightSplitter->setHandleWidth(1);
    m_rightSplitter->setChildrenCollapsible(true);

    m_clarityColumn = new QWidget;
    m_clarityColumn->setStyleSheet(
        "background: #2a2a3c; border-left: 1px solid #313244;"
        " border-right: 1px solid #313244;");
    auto* clarityLayout = new QVBoxLayout(m_clarityColumn);
    clarityLayout->setContentsMargins(0, 0, 0, 0);
    clarityLayout->setSpacing(0);
    m_clarityPanel = new ClarityPanel;
    clarityLayout->addWidget(m_clarityPanel);
    m_rightSplitter->addWidget(m_clarityColumn);

    m_chatColumn = new QWidget;
    m_chatColumn->setStyleSheet("background: #2a2a3c;");
    auto* chatLayout = new QVBoxLayout(m_chatColumn);
    chatLayout->setContentsMargins(0, 0, 0, 0);
    chatLayout->setSpacing(0);
    m_aiChatPanel = new AIChatPanel;
    m_aiChatPanel->setEditorForTracking(m_editor);
    chatLayout->addWidget(m_aiChatPanel);
    m_rightSplitter->addWidget(m_chatColumn);

    m_rightSplitter->setSizes({200, 200});
    m_rightSplitter->setStretchFactor(0, 1);
    m_rightSplitter->setStretchFactor(1, 1);

    m_mainSplitter->addWidget(m_rightSplitter);

    m_mainSplitter->setSizes({200, 680, 400});
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setStretchFactor(2, 0);

    mainLayout->addWidget(m_mainSplitter, 1);

    // Analysis frames — all hidden by default, live in the same layout slot
    m_securityFrame = new SecurityTestingFrame;
    m_securityFrame->setVisible(false);
    mainLayout->addWidget(m_securityFrame);

    m_runtimeAnalysis = new RuntimeAnalysisFrame;
    m_runtimeAnalysis->setVisible(false);
    mainLayout->addWidget(m_runtimeAnalysis);

    m_memoryAnalysis = new MemoryAnalysisFrame;
    m_memoryAnalysis->setVisible(false);
    mainLayout->addWidget(m_memoryAnalysis);

    m_depAnalysis = new DependencyAnalysisFrame;
    m_depAnalysis->setVisible(false);
    mainLayout->addWidget(m_depAnalysis);

    m_codeHealth = new CodeHealthFrame;
    m_codeHealth->setVisible(false);
    mainLayout->addWidget(m_codeHealth);

    m_debugFrame = new DebugFrame;
    m_debugFrame->setVisible(false);
    mainLayout->addWidget(m_debugFrame);

    m_dataTrace = new DataTraceFrame;
    m_dataTrace->setVisible(false);
    mainLayout->addWidget(m_dataTrace);

    m_errorJournal = new ErrorJournal;
    m_errorJournal->setVisible(false);
    mainLayout->addWidget(m_errorJournal);

    m_typeFlow = new TypeFlowFrame;
    m_typeFlow->setVisible(false);
    mainLayout->addWidget(m_typeFlow);

    m_machineView = new MachineViewPanel;
    m_machineView->setVisible(false);
    mainLayout->addWidget(m_machineView);

    m_securityLab = nullptr;  // built on demand

    m_predictPanel = new PredictPanel;
    mainLayout->addWidget(m_predictPanel);

    m_buildBar = new BuildBar;
    mainLayout->addWidget(m_buildBar);

    setCentralWidget(centralWidget);

    // Overlays (parented to central widget so they float above)
    m_langGuide       = new LangGuideOverlay(centralWidget);
    m_basicsOverlay   = new BasicsOverlay(centralWidget);
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

    // Connect welcome page buttons
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
            if (m_aiChatPanel) m_aiChatPanel->setWorkingDirectory(dir);
            if (m_lspManager) m_lspManager->setWorkspaceRoot(dir);
        }
    });


    // ── Build system wiring ──────────────────────────────────────────────
    connect(m_buildSystem, &BuildSystem::outputReady, this,
        [this](const QString& html) {
            QTextEdit* te = m_buildBar->resultsContent();
            if (te) {
                te->moveCursor(QTextCursor::End);
                te->insertHtml(html);
            }
        });

    connect(m_buildSystem, &BuildSystem::finished, this,
        [this](bool success) {
            Q_UNUSED(success);
        });

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
                QString filePath = m_editor ? m_editor->currentFilePath() : QString();
                QString lang = currentLangKey();
                m_buildBar->clearResults();

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

    connect(m_buildBar, &BuildBar::runRequested, this, [this]() {
        if (!m_editor) return;
        QString filePath = m_editor->currentFilePath();
        if (filePath.isEmpty()) {
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

    connect(m_buildBar, &BuildBar::buildRequested, this, &MainWindow::runBuildChain);

    auto openSettingsDialog = [this]() {
        SettingsPanel dlg(this);
        connect(&dlg, &SettingsPanel::modelsChanged, this, [this]() {
            if (m_aiChatPanel) m_aiChatPanel->reloadModelFromConfig();
        });
        connect(&dlg, &SettingsPanel::menuVisibilityChanged, this, [this]() {
            applyToolsMenuVisibility();
        });
        connect(&dlg, &SettingsPanel::tickerSettingsChanged, this, [this]() {
            if (m_newsTicker) m_newsTicker->applySettings();
        });
        connect(&dlg, &SettingsPanel::cveSettingsChanged, this, [this]() {
            if (m_cveMonitor) {
                m_cveMonitor->setFrequencyMs(CVEMonitor::loadFrequencyMs());
                m_cveMonitor->setEnabled(CVEMonitor::loadEnabled());
            }
            if (m_securityFrame) m_securityFrame->reloadCVESettings();
        });
        dlg.exec();
        if (m_aiChatPanel) m_aiChatPanel->reloadModelFromConfig();
        updateDidYouKnowBanner();
        if (m_buildBar) m_buildBar->updatePredictVisibility();
    };
    connect(m_buildBar, &BuildBar::customizeLayoutRequested, this, openSettingsDialog);

    // Register all tools with the favorites bar (after all widgets are created)
    setupFavoritesBar();
}
