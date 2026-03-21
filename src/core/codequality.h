#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QString>
#include <QList>

// ============================================================================
// Data types
// ============================================================================

enum class HealthScore { Green, Yellow, Red };

struct HealthFinding {
    QString subject;
    QString detail;
    int     score = 0;
};

struct MetricResult {
    QString              name;
    HealthScore          score   = HealthScore::Green;
    QString              summary;
    QList<HealthFinding> findings;
};

struct HealthReport {
    MetricResult complexity;
    MetricResult readability;
    MetricResult duplication;
    MetricResult deadCode;
    MetricResult naming;
    QChar        grade;
    HealthScore  overallScore;
};

struct OptimizationEntry {
    QString title;
    QString description;
    QString whyItMatters;
    QString originalCode;
    QString suggestedCode;
    int     lineNumber = -1;
};

// ============================================================================
// CodeQualityEngine — all analysis in one place
//
// Loads patterns from:
//   data/patterns_optimizer.json   — optimizer pattern definitions
//   data/patterns_codehealth.json  — health metric thresholds / messages
//
// Usage:
//   CodeQualityEngine engine;
//   engine.setCode(code, language, level);
//   FullQualityReport r = engine.runFullReport();
// ============================================================================

struct FullQualityReport {
    HealthReport             health;
    QList<OptimizationEntry> optimizations;
    QChar                    grade;        // A-F overall
    QChar                    healthGrade;
    QChar                    optGrade;
};

class CodeQualityEngine {
public:
    CodeQualityEngine() = default;

    // --- Pattern scanning (optimizer) ---
    QList<OptimizationEntry> analyzeFile(const QString& code,
                                         const QString& language,
                                         int level = 1) const;

    static QString crossLanguageNote(const QString& fasterLang,
                                     double speedRatio,
                                     int level = 1);

    // --- Health metrics ---
    static HealthReport buildHealthReport(const QStringList& lines,
                                          const QString& lang);

    // --- Combined ---
    void              setCode(const QString& code, const QString& language, int level = 1);
    FullQualityReport runFullReport() const;

private:
    // Optimizer passes
    QList<OptimizationEntry> analyzePython(const QStringList& lines, int level) const;
    QList<OptimizationEntry> analyzeCpp(const QStringList& lines, bool isCpp, int level) const;
    bool nameUsedAfterLine(const QStringList& lines, const QString& name, int defLine) const;

    // Health passes (static — no instance state needed)
    static MetricResult analyzeComplexity(const QStringList& lines, const QString& lang);
    static MetricResult analyzeReadability(const QStringList& lines, const QString& lang);
    static MetricResult analyzeDuplication(const QStringList& lines);
    static MetricResult analyzeDeadCode(const QStringList& lines, const QString& lang);
    static MetricResult analyzeNaming(const QStringList& lines, const QString& lang);

    // Scoring
    static int   healthScoreToPoints(HealthScore s);
    static QChar computeGrade(int healthPoints, int optimCount);

    QString m_code;
    QString m_language;
    int     m_level = 1;
};

// ============================================================================
// Free helpers (keep old names for callers in codehealth / fullreport)
// ============================================================================
QString    healthScoreColor(HealthScore s);
HealthReport healthBuildReport(const QStringList& lines, const QString& lang);

// ============================================================================
// MetricCard — clickable card widget (used by CodeHealthFrame)
// ============================================================================
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

// ============================================================================
// CodeHealthFrame — "report card" panel (5 metric cards + grade)
// ============================================================================
class CodeHealthFrame : public QWidget {
    Q_OBJECT
public:
    explicit CodeHealthFrame(QWidget* parent = nullptr);

    void setCode(const QString& code, const QString& language);

    // Keep old static entry point so FullReportFrame can call it directly
    static HealthReport buildReport(const QStringList& lines, const QString& lang);

signals:
    void backToEditor();

private slots:
    void onRunHealth();
    void onHelpClicked();
    void onCardExpanded(const MetricResult& result);

private:
    void populateGrade(const HealthReport& report);
    void populateCards(const HealthReport& report);
    void showFindings(const MetricResult& result);

    static QString scoreColor(HealthScore s);
    static QString gradeColor(HealthScore s);

    QLabel*      m_gradeLabel      = nullptr;
    QLabel*      m_gradeCaption    = nullptr;
    QPushButton* m_runBtn          = nullptr;
    QLabel*      m_statusLabel     = nullptr;
    QWidget*     m_cardGrid        = nullptr;
    QScrollArea* m_findingsScroll  = nullptr;
    QWidget*     m_findingsContent = nullptr;
    QLabel*      m_findingsTitle   = nullptr;

    QString m_code;
    QString m_language;
};

// ============================================================================
// FullReportFrame — combined A-F score (health + optimizer)
// ============================================================================
class FullReportFrame : public QWidget {
    Q_OBJECT
public:
    explicit FullReportFrame(QWidget* parent = nullptr);

    void setCode(const QString& code, const QString& language, int level = 1);

signals:
    void backToEditor();

private slots:
    void onRunReport();
    void onHelpClicked();

private:
    static int     healthScoreToPoints(HealthScore s);
    static QChar   computeGrade(int healthPoints, int optimCount);
    static QString gradeColor(QChar grade);

    void buildScoreHeader(QChar grade, QChar healthGrade, QChar optGrade);
    void buildHealthSection(const HealthReport& report);
    void buildOptimizerSection(const QList<OptimizationEntry>& entries);
    void clearBody();

    QPushButton* m_runBtn        = nullptr;
    QLabel*      m_gradeLabel    = nullptr;
    QLabel*      m_subGradeLabel = nullptr;
    QWidget*     m_bodyWidget    = nullptr;
    QScrollArea* m_scrollArea    = nullptr;
    QVBoxLayout* m_bodyLayout    = nullptr;

    QString m_code;
    QString m_language;
    int     m_level = 1;
};
