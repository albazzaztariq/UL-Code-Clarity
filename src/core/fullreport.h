#pragma once

#include <QWidget>
#include <QLabel>
#include <QScrollArea>
#include <QPushButton>
#include <QString>
#include <QList>

#include "core/codehealth.h"
#include "core/optimizer.h"

// ============================================================================
// FullReportFrame — unified A-F score combining CodeHealth + Optimizer results
//
// Shown as a full-screen overlay replacing the editor.
// Layout:
//   [Back]             Full Report             [?]
//   ┌─────────────────────────────────────────────┐
//   │  Overall Score: B+   (large, coloured)      │
//   │  Code Health: B  |  Optimizer: A            │
//   └─────────────────────────────────────────────┘
//   [Run Full Report]
//   ── Code Health ──────────────────────────────
//   Complexity: Green   Readability: Yellow  ...
//   ── Optimizations ────────────────────────────
//   • Unused import (line 3)
//   • String concat in loop (line 17)
//   ...
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
    // Scoring helpers
    static int healthScoreToPoints(HealthScore s);   // Green=2, Yellow=1, Red=0
    static QChar computeGrade(int healthPoints, int optimCount);
    static QString gradeColor(QChar grade);

    // UI builders
    void buildScoreHeader(QChar grade, QChar healthGrade, QChar optGrade);
    void buildHealthSection(const HealthReport& report);
    void buildOptimizerSection(const QList<OptimizationEntry>& entries);
    void clearBody();

    // Widgets
    QPushButton* m_runBtn         = nullptr;
    QLabel*      m_gradeLabel     = nullptr;
    QLabel*      m_subGradeLabel  = nullptr;
    QWidget*     m_bodyWidget     = nullptr;
    QScrollArea* m_scrollArea     = nullptr;
    QVBoxLayout* m_bodyLayout     = nullptr;

    // State
    QString m_code;
    QString m_language;
    int     m_level = 1;
};
