#include "core/fullreport.h"
#include "core/theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QPushButton>
#include <QLabel>

using namespace Theme::Css;

// ============================================================================
// FullReportFrame — constructor
// ============================================================================

FullReportFrame::FullReportFrame(QWidget* parent)
    : AnalysisFrame("Full Report + Score", parent)
{
    // ── Score header (unique) ──────────────────────────────────────────────────
    auto* scoreArea = new QWidget;
    scoreArea->setFixedHeight(100);
    scoreArea->setStyleSheet(QString("background: %1; border-bottom: 1px solid #313244;").arg(BG5));
    auto* scoreLayout = new QVBoxLayout(scoreArea);
    scoreLayout->setContentsMargins(0, 8, 0, 8);
    scoreLayout->setSpacing(4);
    scoreLayout->setAlignment(Qt::AlignCenter);

    m_gradeLabel = new QLabel("--");
    m_gradeLabel->setStyleSheet(QString("color: %1; font-size: 48px; font-weight: bold;"
                                " background: transparent;").arg(FG));
    m_gradeLabel->setAlignment(Qt::AlignCenter);
    scoreLayout->addWidget(m_gradeLabel);

    m_subGradeLabel = new QLabel("Run a report to see your score");
    m_subGradeLabel->setStyleSheet("color: #585b70; font-size: 12px; background: transparent;");
    m_subGradeLabel->setAlignment(Qt::AlignCenter);
    scoreLayout->addWidget(m_subGradeLabel);

    m_resultsLayout->insertWidget(0, scoreArea);

    // ── Run button ────────────────────────────────────────────────────────────
    auto* btnRow = new QWidget;
    btnRow->setFixedHeight(48);
    btnRow->setStyleSheet(QString("background: %1;").arg(BG));
    auto* btnLayout = new QHBoxLayout(btnRow);
    btnLayout->setContentsMargins(16, 8, 16, 8);

    m_runBtn = new QPushButton("Run Full Report");
    m_runBtn->setCursor(Qt::PointingHandCursor);
    m_runBtn->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border-radius: 6px;"
        " font-size: 13px; font-weight: bold; padding: 6px 20px; border: none; }"
        "QPushButton:hover { background: #b4d0fb; }").arg(ACCENT, BG));
    connect(m_runBtn, &QPushButton::clicked, this, &FullReportFrame::onRunReport);
    btnLayout->addStretch();
    btnLayout->addWidget(m_runBtn);
    btnLayout->addStretch();

    m_resultsLayout->insertWidget(1, btnRow);
}

// ============================================================================
// Public API
// ============================================================================

void FullReportFrame::setCode(const QString& code, const QString& language, int level)
{
    m_code     = code;
    m_language = language;
    m_level    = level;
}

// ============================================================================
// Scoring helpers
// ============================================================================

int FullReportFrame::healthScoreToPoints(HealthScore s)
{
    switch (s) {
    case HealthScore::Green:  return 2;
    case HealthScore::Yellow: return 1;
    case HealthScore::Red:    return 0;
    }
    return 1;
}

QChar FullReportFrame::computeGrade(int healthPoints, int optimCount)
{
    // healthPoints: 0-10 (5 metrics × 0-2)
    // optimCount: number of optimizer issues (lower = better)
    int score = healthPoints * 10;             // 0-100
    score -= qMin(optimCount * 5, 40);         // subtract up to 40 for issues

    if (score >= 85) return QChar('A');
    if (score >= 70) return QChar('B');
    if (score >= 55) return QChar('C');
    if (score >= 40) return QChar('D');
    return QChar('F');
}

QString FullReportFrame::gradeColor(QChar grade)
{
    if (grade == 'A') return GREEN;
    if (grade == 'B') return TEAL;
    if (grade == 'C') return YELLOW;
    if (grade == 'D') return PEACH;
    return RED;
}

// ============================================================================
// Slots
// ============================================================================

void FullReportFrame::onRunReport()
{
    if (m_code.isEmpty()) return;

    QStringList lines = m_code.split('\n');

    // Run health analysis
    HealthReport health = healthBuildReport(lines, m_language);

    // Run optimizer
    CodeOptimizer opt;
    QList<OptimizationEntry> optEntries = opt.analyzeFile(m_code, m_language, m_level);

    // Compute health grade (A-F from existing grade field)
    QChar healthGrade = health.grade;

    // Compute optimizer grade
    int optScore = qMax(0, 100 - optEntries.size() * 8);
    QChar optGrade;
    if (optScore >= 85)      optGrade = QChar('A');
    else if (optScore >= 70) optGrade = QChar('B');
    else if (optScore >= 55) optGrade = QChar('C');
    else if (optScore >= 40) optGrade = QChar('D');
    else                     optGrade = QChar('F');

    // Combine into overall grade
    int healthPoints =
        healthScoreToPoints(health.complexity.score)  +
        healthScoreToPoints(health.readability.score) +
        healthScoreToPoints(health.duplication.score) +
        healthScoreToPoints(health.deadCode.score)    +
        healthScoreToPoints(health.naming.score);

    QChar overall = computeGrade(healthPoints, optEntries.size());

    // Rebuild UI
    clearBody();
    buildScoreHeader(overall, healthGrade, optGrade);
    buildHealthSection(health);
    buildOptimizerSection(optEntries);
}

void FullReportFrame::onHelpClicked()
{
    // Reuse standard help styling from other panels
    auto* dlg = new QWidget(this, Qt::Tool | Qt::WindowStaysOnTopHint);
    dlg->setWindowTitle("Full Report Help");
    dlg->resize(380, 260);
    dlg->setStyleSheet(QString("background: %1; color: %2;").arg(BG, FG));
    auto* l = new QVBoxLayout(dlg);
    auto* lbl = new QLabel(
        "<b>Full Report</b><br><br>"
        "Combines <b>Code Health</b> (complexity, readability, duplication, "
        "dead code, naming) and <b>Optimizer</b> analysis into a single A-F score.<br><br>"
        "<b>Score breakdown:</b><br>"
        "• A — excellent: clean structure, minimal issues<br>"
        "• B — good: minor improvements possible<br>"
        "• C — fair: some patterns to address<br>"
        "• D — poor: significant issues found<br>"
        "• F — critical: major structural problems<br><br>"
        "Set your Assist Level before running to tune the detail of explanations."
    );
    lbl->setWordWrap(true);
    lbl->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(FG));
    l->addWidget(lbl);
    auto* closeBtn = new QPushButton("Close");
    closeBtn->setStyleSheet(
        QString("QPushButton { background: #313244; color: %1; border: none;"
        " border-radius: 4px; padding: 4px 12px; } "
        "QPushButton:hover { background: %2; }").arg(FG, BORDER));
    connect(closeBtn, &QPushButton::clicked, dlg, &QWidget::close);
    l->addWidget(closeBtn);
    dlg->show();
}

// ============================================================================
// UI builders
// ============================================================================

void FullReportFrame::clearBody()
{
    // Remove all items after the scoreArea (index 0) and btnRow (index 1)
    while (m_resultsLayout->count() > 2) {
        QLayoutItem* item = m_resultsLayout->takeAt(2);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}

void FullReportFrame::buildScoreHeader(QChar grade, QChar healthGrade, QChar optGrade)
{
    m_gradeLabel->setText(grade);
    m_gradeLabel->setStyleSheet(
        QString("color: %1; font-size: 48px; font-weight: bold; background: transparent;")
            .arg(gradeColor(grade)));
    m_subGradeLabel->setText(
        QString("Code Health: %1   |   Optimizer: %2")
            .arg(healthGrade).arg(optGrade));
    m_subGradeLabel->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(FG2));
}

static QWidget* makeSectionHeader(const QString& title)
{
    auto* w = new QWidget;
    w->setFixedHeight(28);
    w->setStyleSheet("background: transparent;");
    auto* l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(8);

    auto* lbl = new QLabel(title);
    lbl->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold; background: transparent;").arg(ACCENT));
    l->addWidget(lbl);

    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: #313244; background: #313244;");  // surface0, no Css constant
    l->addWidget(line, 1);

    return w;
}

static QWidget* makeMetricRow(const MetricResult& m)
{
    auto* w = new QWidget;
    w->setStyleSheet(QString("background: %1; border-radius: 6px;").arg(BG5));
    auto* l = new QHBoxLayout(w);
    l->setContentsMargins(12, 8, 12, 8);
    l->setSpacing(12);

    QString dot;
    switch (m.score) {
    case HealthScore::Green:  dot = GREEN;  break;
    case HealthScore::Yellow: dot = YELLOW; break;
    case HealthScore::Red:    dot = RED;    break;
    }
    auto* dotLbl = new QLabel("●");
    dotLbl->setStyleSheet(QString("color: %1; font-size: 14px; background: transparent;").arg(dot));
    dotLbl->setFixedWidth(16);
    l->addWidget(dotLbl);

    auto* nameLbl = new QLabel(m.name);
    nameLbl->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold; background: transparent;").arg(FG));
    nameLbl->setFixedWidth(110);
    l->addWidget(nameLbl);

    auto* sumLbl = new QLabel(m.summary);
    sumLbl->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent;").arg(FG2));
    sumLbl->setWordWrap(true);
    l->addWidget(sumLbl, 1);

    return w;
}

static QWidget* makeOptRow(const OptimizationEntry& e)
{
    auto* w = new QWidget;
    w->setStyleSheet(QString("background: %1; border-radius: 6px;").arg(BG5));
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(12, 8, 12, 8);
    l->setSpacing(4);

    auto* titleRow = new QHBoxLayout;
    titleRow->setSpacing(8);

    auto* lineLbl = new QLabel(e.lineNumber > 0
        ? QString("Line %1").arg(e.lineNumber) : QString("—"));
    lineLbl->setFixedWidth(50);
    lineLbl->setStyleSheet("color: #585b70; font-size: 10px; background: transparent;");  // overlay0, no Css constant
    titleRow->addWidget(lineLbl);

    auto* titleLbl = new QLabel(e.title);
    titleLbl->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold; background: transparent;").arg(ACCENT));
    titleRow->addWidget(titleLbl, 1);

    l->addLayout(titleRow);

    auto* descLbl = new QLabel(e.description);
    descLbl->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent;").arg(FG2));
    descLbl->setWordWrap(true);
    l->addWidget(descLbl);

    return w;
}

void FullReportFrame::buildHealthSection(const HealthReport& report)
{
    m_resultsLayout->addWidget(makeSectionHeader("Code Health"));

    for (const MetricResult* m : {
            &report.complexity, &report.readability, &report.duplication,
            &report.deadCode,   &report.naming }) {
        m_resultsLayout->addWidget(makeMetricRow(*m));
    }
}

void FullReportFrame::buildOptimizerSection(const QList<OptimizationEntry>& entries)
{
    m_resultsLayout->addWidget(makeSectionHeader("Optimizer"));

    if (entries.isEmpty()) {
        auto* noneLbl = new QLabel("No optimization issues found.");
        noneLbl->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(GREEN));
        noneLbl->setContentsMargins(4, 4, 4, 4);
        m_resultsLayout->addWidget(noneLbl);
        return;
    }

    for (const OptimizationEntry& e : entries)
        m_resultsLayout->addWidget(makeOptRow(e));
}
