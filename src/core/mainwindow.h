#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QLabel>
#include <QMenu>
#include <QCloseEvent>
#include <QPushButton>
#include "core/aipermissions.h"

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

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    void closeEvent(QCloseEvent* event) override;

    EditorWidget*   editor()        const { return m_editor; }
    FileTreeWidget* fileTree()      const { return m_fileTree; }
    BuildBar*       buildBar()      const { return m_buildBar; }
    RuntimeStrip*   runtimeStrip()  const { return m_runtimeStrip; }
    QWidget*        clarityColumn() const { return m_clarityColumn; }
    QWidget*        chatColumn()    const { return m_chatColumn; }

private:
    void createMenuBar();
    void createStatusBar();
    void setupCentralLayout();
    void wireSignals();
    void saveSession();
    void restoreSession();
    void addToRecentWorkspaces(const QString& path);
    void rebuildRecentWorkspacesMenu();
    void applyTheme();
    QString currentLangKey() const;  // map editor language display → buildsystem key

    EditorWidget*   m_editor      = nullptr;
    FileTreeWidget* m_fileTree    = nullptr;
    BuildBar*       m_buildBar    = nullptr;
    RuntimeStrip*   m_runtimeStrip = nullptr;

    // Right panel: two side-by-side columns (Clarity + AI Chat)
    QSplitter*      m_rightSplitter = nullptr;
    QWidget*        m_clarityColumn = nullptr;  // panels agent fills this
    QWidget*        m_chatColumn    = nullptr;   // panels agent fills this
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

    // Recent Workspaces submenu
    QMenu* m_recentMenu = nullptr;

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

    // Permissions system
    AIPermissions m_aiPermissions;

    // Build system — toolchain detection + run/build execution
    BuildSystem*  m_buildSystem = nullptr;

    // Explain button state: stored original code before inline annotation
    QString m_preExplainCode;
    bool    m_explainActive = false;
};
