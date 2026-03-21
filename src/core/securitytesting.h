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
// Cards are color-coded by severity. Clicking the line number emits jumpToLine.
// ============================================================================
class SecurityTestingFrame : public QWidget {
    Q_OBJECT

public:
    explicit SecurityTestingFrame(QWidget* parent = nullptr);

    // Call before showing the frame so SAST has the current code + language
    void setCode(const QString& code, const QString& language);

    // Call before DAST so it knows the compiled exe path
    void setExePath(const QString& exePath);

signals:
    void backToEditor();
    void jumpToLine(int lineNumber);   // user clicked a line-number label on a card

private slots:
    void onRunSast();
    void onRunDast();
    void onHelpClicked();

private:
    // Card builder — returns a styled QFrame for one SecurityFinding
    QWidget* buildCard(const SecurityFinding& f, bool showJump = true);

    // Populate a scroll area with cards; clears previous content first
    void populateResults(QScrollArea* area, const QList<SecurityFinding>& findings);

    // Color for severity
    static QString severityColor(SecurityFinding::Severity sev);
    static QString severityLabel(SecurityFinding::Severity sev);

    // ── Widgets ──────────────────────────────────────────────────────────────
    QTabWidget*  m_tabs          = nullptr;

    // SAST tab
    QWidget*     m_sastTab       = nullptr;
    QPushButton* m_runSastBtn    = nullptr;
    QScrollArea* m_sastScroll    = nullptr;
    QLabel*      m_sastStatus    = nullptr;

    // DAST tab
    QWidget*     m_dastTab       = nullptr;
    QPushButton* m_runDastBtn    = nullptr;
    QScrollArea* m_dastScroll    = nullptr;
    QLabel*      m_dastStatus    = nullptr;

    // ── State ────────────────────────────────────────────────────────────────
    QString m_code;
    QString m_language;
    QString m_exePath;
};
