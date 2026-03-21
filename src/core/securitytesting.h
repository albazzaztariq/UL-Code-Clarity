#pragma once

#include "core/sast.h"
#include <QWidget>
#include <QTabWidget>
#include <QPushButton>
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

signals:
    void backToEditor();
    void jumpToLine(int lineNumber);
    void openLab(const QString& vulnType);  // emitted when "Try It Yourself" clicked

private slots:
    void onRunSast();
    void onRunDast();
    void onHelpClicked();

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

    // ── State ────────────────────────────────────────────────────────────────
    QString m_code;
    QString m_language;
    QString m_filePath;
    QString m_exePath;
};
