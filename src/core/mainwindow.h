#pragma once

#include <functional>
#include <QMainWindow>
#include <QSplitter>
#include <QLabel>
#include <QMenu>
#include <QCloseEvent>
#include <QPushButton>
#include <QMap>
#include <QAction>
#include "core/aipermissions.h"
#include "core/custompipeline.h"
#include "core/favoritesbar.h"
#include "core/buildchain.h"

class BuildSystem;
class EditorWidget;
class FileTreeWidget;
class BuildBar;
class RuntimeStrip;
class ClarityPanel;
class AIChatPanel;
class LevelSelector;
class LangGuideOverlay;
class BasicsOverlay;
class ExpanderOverlay;
class ExplainHandler;
class WalkThroughHandler;
class BuildFromScratchMode;
class SecurityTestingFrame;
class SecurityLabWidget;
class RuntimeAnalysisFrame;
class MemoryAnalysisFrame;
class DependencyAnalysisFrame;
class CodeHealthFrame;
class InteractiveBasicsDialog;
class DebugFrame;
class DataTraceFrame;
class PredictPanel;
class ErrorJournal;
class CostVisualizer;
class CostVisualizerPanel;
class TypeFlowFrame;
class MachineViewPanel;
class CVEMonitor;
class NewsTicker;
class EditorTracker;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

    EditorWidget*   editor()        const { return m_editor; }
    FileTreeWidget* fileTree()      const { return m_fileTree; }
    BuildBar*       buildBar()      const { return m_buildBar; }
    RuntimeStrip*   runtimeStrip()  const { return m_runtimeStrip; }
    QWidget*        clarityColumn() const { return m_clarityColumn; }
    QWidget*        chatColumn()    const { return m_chatColumn; }

private:
    void createMenuBar();
    void createLearningMenu();
    void buildToolsMenu();
    void createStatusBar();
    void setupTitleBar();
    void applyTitleBarTheme();
    void applyWin32Frameless();
    void setupDidYouKnowBanner();
    void updateDidYouKnowBanner();
    QString buildDidYouKnowHtml(bool isDark);
    void setupCentralLayout();
    void wireSignals();
    void saveSession();
    void restoreSession();
    void addToRecentWorkspaces(const QString& path);
    void rebuildRecentWorkspacesMenu();
    void rebuildSavedPipelinesMenu();
    void applyToolsMenuVisibility();
    void applyTheme();
    void setupFavoritesBar();
    void runBuildChain();
    void runPipeline(const PipelineConfig& cfg);
    void hideAllAnalysisFrames();
    void showAnalysisFrame(QWidget* frame);
    void returnToEditor();
    // Connects action->triggered() to showAnalysisFrame(frame) and wires frame's
    // backToEditor-style signal back to returnToEditor().
    // Usage: call once per frame; the signal connector (backFn) is a callable that
    // performs the specific connect() for that frame type's back signal.
    void connectFrameToggle(QAction* action, QWidget* frame,
                            std::function<void()> setupFn,
                            std::function<void()> backFn);
    void showCVENotificationBar(int count, const QStringList& packages);
    QString currentLangKey() const;

    EditorWidget*   m_editor      = nullptr;
    FileTreeWidget* m_fileTree    = nullptr;
    BuildBar*       m_buildBar    = nullptr;
    RuntimeStrip*   m_runtimeStrip = nullptr;

    // Right panel: two side-by-side columns (Clarity + AI Chat)
    QSplitter*      m_rightSplitter = nullptr;
    QWidget*        m_clarityColumn = nullptr;
    QWidget*        m_chatColumn    = nullptr;
    QSplitter*      m_mainSplitter  = nullptr;

    // Panels
    ClarityPanel*       m_clarityPanel  = nullptr;
    AIChatPanel*        m_aiChatPanel   = nullptr;
    LevelSelector*      m_levelSelector = nullptr;
    LangGuideOverlay*   m_langGuide     = nullptr;
    BasicsOverlay*      m_basicsOverlay = nullptr;
    ExpanderOverlay*    m_expanderOverlay = nullptr;
    ExplainHandler*     m_explainHandler = nullptr;
    WalkThroughHandler* m_walkHandler   = nullptr;
    BuildFromScratchMode* m_bfsMode     = nullptr;
    SecurityTestingFrame* m_securityFrame = nullptr;
    SecurityLabWidget*    m_securityLab   = nullptr;
    RuntimeAnalysisFrame* m_runtimeAnalysis = nullptr;
    MemoryAnalysisFrame*  m_memoryAnalysis  = nullptr;
    DependencyAnalysisFrame* m_depAnalysis  = nullptr;
    CodeHealthFrame*      m_codeHealth    = nullptr;
    InteractiveBasicsDialog* m_interactiveBasics = nullptr;
    DebugFrame*           m_debugFrame    = nullptr;
    DataTraceFrame*       m_dataTrace     = nullptr;
    PredictPanel*         m_predictPanel  = nullptr;
    ErrorJournal*         m_errorJournal  = nullptr;
    CostVisualizer*       m_costVisualizer = nullptr;
    CostVisualizerPanel*  m_costPanel      = nullptr;
    TypeFlowFrame*        m_typeFlow       = nullptr;
    MachineViewPanel*     m_machineView    = nullptr;

    // Claude Code integration
    EditorTracker* m_editorTracker = nullptr;

    // News ticker
    NewsTicker*   m_newsTicker    = nullptr;

    // Favorites bar
    FavoritesBar* m_favoritesBar  = nullptr;

    // Build chain
    BuildChainConfig  m_activeBuildChain;

    // CVE monitoring
    CVEMonitor*   m_cveMonitor    = nullptr;
    QWidget*      m_cveBar        = nullptr;
    QLabel*       m_cveBarLabel   = nullptr;
    QStringList   m_cveBarPackages;

    // Menus
    QMenu*  m_recentMenu        = nullptr;
    QMenu*  m_toolsMenu         = nullptr;
    QMenu*  m_savedPipelinesMenu = nullptr;
    QAction* m_viewClarityAct   = nullptr;
    QAction* m_viewAIChatAct    = nullptr;

    // Tools menu actions (keyed by settings key for visibility control)
    QMap<QString, QAction*> m_toolsActions;

    // Status bar labels
    QLabel* m_statusReady = nullptr;
    QLabel* m_statusLang  = nullptr;
    QLabel* m_statusEnc   = nullptr;
    QLabel* m_statusPos   = nullptr;
    QWidget* m_statusBarSpacer = nullptr;

    // Theme toggle
    QPushButton* m_themeToggleBtn = nullptr;
    QPushButton* m_settingsGearBtn = nullptr;
    bool m_isDarkTheme = true;

    // Custom title bar
    QWidget* m_titleBar = nullptr;
    QLabel* m_windowIconLabel = nullptr;
    QLabel* m_windowTitleLabel = nullptr;
    QPushButton* m_minimizeBtn = nullptr;
    QPushButton* m_maximizeBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;

    // Did You Know banner
    QWidget* m_didYouKnowBanner = nullptr;
    QLabel* m_didYouKnowLabel = nullptr;
    QTimer* m_didYouKnowTimer = nullptr;
    int m_lastDidYouKnowIndex = -1;

    // Permissions system
    AIPermissions m_aiPermissions;

    // Build system
    BuildSystem*  m_buildSystem = nullptr;

    // Explain state
    QString m_preExplainCode;
    bool    m_explainActive = false;
};
