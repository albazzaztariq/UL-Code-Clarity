#pragma once

#include "core/analysisframe.h"
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QList>
#include <QString>

// ============================================================================
// CodeHealthFrame — "report card" for your code
//
// Layout:
//   [Back to Editor]          Code Health          [?]
//   ┌──────────────────────────────────────────────────┐
//   │  Overall Grade: A   (large, colored)             │
//   └──────────────────────────────────────────────────┘
//   [Run Health Check]
//   ┌──────────┐ ┌──────────┐ ┌──────────┐
//   │Complexity│ │Readabilty│ │Duplicatn │   row 1
//   └──────────┘ └──────────┘ └──────────┘
//   ┌──────────┐ ┌──────────┐ ┌──────────┐
//   │ Dead Code│ │  Naming  │ │ (spacer) │   row 2
//   └──────────┘ └──────────┘ └──────────┘
//   Click a card to expand its findings list below.
// ============================================================================

// ── Metric score level ───────────────────────────────────────────────────────
enum class HealthScore { Green, Yellow, Red };

// ── One finding item within a metric ────────────────────────────────────────
struct HealthFinding {
    QString subject;    // e.g. function name, variable name, block preview
    QString detail;     // human-readable explanation
    int     score = 0;  // numeric value if applicable (complexity count, line count, …)
};

// ── Result for one metric ────────────────────────────────────────────────────
struct MetricResult {
    QString              name;
    HealthScore          score  = HealthScore::Green;
    QString              summary;           // short status line shown on card
    QList<HealthFinding> findings;          // expanded list
};

// ── Full report ──────────────────────────────────────────────────────────────
struct HealthReport {
    MetricResult complexity;
    MetricResult readability;
    MetricResult duplication;
    MetricResult deadCode;
    MetricResult naming;
    QChar        grade;          // A / B / C / D / F
    HealthScore  overallScore;   // drives grade colour
};

// Free helpers used by MetricCard, CodeHealthFrame, and FullReportFrame
QString healthScoreColor(HealthScore s);
HealthReport healthBuildReport(const QStringList& lines, const QString& lang);

// ── Metric card widget (click to expand/collapse) ────────────────────────────
class MetricCard : public QWidget {
    Q_OBJECT
public:
    explicit MetricCard(const MetricResult& result, QWidget* parent = nullptr);

signals:
    void expandRequested(const MetricResult& result);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    MetricResult m_result;
};

// ── Main frame ───────────────────────────────────────────────────────────────
class CodeHealthFrame : public AnalysisFrame {
    Q_OBJECT

public:
    explicit CodeHealthFrame(QWidget* parent = nullptr);

    // Provide code + language before showing
    void setCode(const QString& code, const QString& language);

    // Public for use by FullReportFrame via healthBuildReport()
    static HealthReport buildReport(const QStringList& lines, const QString& lang);

private slots:
    void onRunHealth();
    void onHelpClicked();
    void onCardExpanded(const MetricResult& result);

private:
    // ── Analysis passes ──────────────────────────────────────────────────────
    static MetricResult analyzeComplexity(const QStringList& lines, const QString& lang);
    static MetricResult analyzeReadability(const QStringList& lines, const QString& lang);
    static MetricResult analyzeDuplication(const QStringList& lines);
    static MetricResult analyzeDeadCode(const QStringList& lines, const QString& lang);
    static MetricResult analyzeNaming(const QStringList& lines, const QString& lang);

    // ── UI helpers ───────────────────────────────────────────────────────────
    void populateGrade(const HealthReport& report);
    void populateCards(const HealthReport& report);
    void showFindings(const MetricResult& result);

    static QString scoreColor(HealthScore s);
    static QString gradeColor(HealthScore s);

    // ── Widgets ──────────────────────────────────────────────────────────────
    QLabel*      m_gradeLabel   = nullptr;
    QLabel*      m_gradeCaption = nullptr;
    QPushButton* m_runBtn       = nullptr;

    // Card grid area
    QWidget*     m_cardGrid     = nullptr;

    // Expandable findings panel (below grid)
    QScrollArea* m_findingsScroll  = nullptr;
    QWidget*     m_findingsContent = nullptr;
    QLabel*      m_findingsTitle   = nullptr;
};
