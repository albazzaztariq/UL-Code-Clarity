#pragma once

#include "core/sast.h"
#include "core/cvemonitor.h"
#include <QWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QList>

// ============================================================================
// SecurityTestingFrame — full-frame widget (replaces editor like Runtime Analysis)
//
// Layout:
//   [Back to Editor]                              [?]
//   ┌─────────────────────────────────────────────┐
//   │  Tab: Static Analysis (SAST)                │
//   │  Tab: Dynamic Testing (DAST)                │
//   └─────────────────────────────────────────────┘
//
// Each tab has a "Run" button and a scrollable list of SecurityFinding cards.
// Cards are color-coded by severity and include a "Why This Matters" section.
// Clicking a line number emits jumpToLine. Attribution link at bottom of results.
// ============================================================================
class SecurityTestingFrame : public QWidget {
    Q_OBJECT

public:
    explicit SecurityTestingFrame(QWidget* parent = nullptr);

    // Call before showing the frame so SAST has the current code + language
    void setCode(const QString& code, const QString& language,
                 const QString& filePath = QString());

    // Call before DAST so it knows the compiled exe path
    void setExePath(const QString& exePath);

    // Wire an existing CVEMonitor into this frame (called from MainWindow)
    void setCVEMonitor(CVEMonitor* monitor);

    // Sync CVE controls from QSettings (call when settings change externally)
    void reloadCVESettings();

signals:
    void backToEditor();
    void jumpToLine(int lineNumber);
    void openLab(const QString& vulnType);  // emitted when "Try It Yourself" clicked
    // Emitted when user changes CVE settings in this frame
    void cveSettingsChanged();

private slots:
    void onRunSast();
    void onRunDast();
    void onHelpClicked();
    void onCVEToggled(bool enabled);
    void onCVEFrequencyChanged(int index);
    void onRunCVENow();
    void onCVEAuditCompleted(int count, const QStringList& packages);

private:
    QWidget* buildCard(const SecurityFinding& f);
    void populateResults(QScrollArea* area, const QList<SecurityFinding>& findings,
                         const QString& attributionName, const QString& attributionUrl);

    static QString severityColor(SecurityFinding::Severity sev);
    static QString severityLabel(SecurityFinding::Severity sev);

    // ── Widgets ──────────────────────────────────────────────────────────────
    QTabWidget*  m_tabs       = nullptr;

    QWidget*     m_sastTab    = nullptr;
    QPushButton* m_runSastBtn = nullptr;
    QScrollArea* m_sastScroll = nullptr;
    QLabel*      m_sastStatus = nullptr;

    QWidget*     m_dastTab    = nullptr;
    QPushButton* m_runDastBtn = nullptr;
    QScrollArea* m_dastScroll = nullptr;
    QLabel*      m_dastStatus = nullptr;

    // ── CVE Auto-Check Tab ────────────────────────────────────────────────────
    QWidget*     m_cveTab         = nullptr;
    QCheckBox*   m_cveEnabledChk  = nullptr;
    QComboBox*   m_cveFreqCombo   = nullptr;
    QLabel*      m_cveStatusLabel = nullptr;
    QPushButton* m_cveRunNowBtn   = nullptr;
    CVEMonitor*  m_cveMonitor     = nullptr;  // not owned — set by MainWindow

    // ── State ────────────────────────────────────────────────────────────────
    QString m_code;
    QString m_language;
    QString m_filePath;
    QString m_exePath;
};
