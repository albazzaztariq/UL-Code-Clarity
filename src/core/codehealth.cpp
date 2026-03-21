#include "core/codehealth.h"
#include "core/tutorial.h"
#include "core/jsonloader.h"

#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QDialog>
#include <QRegularExpression>
#include <QSet>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Config helpers — load metric config from patterns_codehealth.json
// ─────────────────────────────────────────────────────────────────────────────

static QJsonObject s_config;

static const QJsonObject& config()
{
    if (s_config.isEmpty())
        s_config = JsonLoader::loadObject("patterns_codehealth.json");
    return s_config;
}

static QJsonObject metricConfig(const QString& id)
{
    for (const QJsonValue& v : config()["metrics"].toArray()) {
        QJsonObject m = v.toObject();
        if (m["id"].toString() == id) return m;
    }
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// Free colour helper
// ─────────────────────────────────────────────────────────────────────────────
QString healthScoreColor(HealthScore s)
{
    switch (s) {
    case HealthScore::Green:  return "#a6e3a1";
    case HealthScore::Yellow: return "#f9e2af";
    case HealthScore::Red:    return "#f38ba8";
    }
    return "#a6e3a1";
}

HealthReport healthBuildReport(const QStringList& lines, const QString& lang)
{
    return CodeHealthFrame::buildReport(lines, lang);
}

QString CodeHealthFrame::scoreColor(HealthScore s) { return healthScoreColor(s); }
QString CodeHealthFrame::gradeColor(HealthScore s) { return healthScoreColor(s); }

// ─────────────────────────────────────────────────────────────────────────────
// MetricCard
// ─────────────────────────────────────────────────────────────────────────────
MetricCard::MetricCard(const MetricResult& result, QWidget* parent)
    : QWidget(parent), m_result(result)
{
    QString color = healthScoreColor(result.score);

    auto* frame = new QFrame(this);
    frame->setStyleSheet(QString(
        "QFrame { background: #1e1e2e; border: 1px solid %1;"
        " border-top: 3px solid %1; border-radius: 6px; }").arg(color));

    auto* fl = new QVBoxLayout(frame);
    fl->setContentsMargins(12, 10, 12, 10);
    fl->setSpacing(6);

    auto* nameRow = new QHBoxLayout;
    nameRow->setSpacing(6);

    auto* dot = new QLabel;
    dot->setFixedSize(12, 12);
    dot->setStyleSheet(QString("background: %1; border-radius: 6px;").arg(color));
    nameRow->addWidget(dot);

    auto* nameLabel = new QLabel(result.name);
    nameLabel->setStyleSheet("color: #cdd6f4; font-size: 13px; font-weight: bold;");
    nameRow->addWidget(nameLabel, 1);
    fl->addLayout(nameRow);

    auto* sumLabel = new QLabel(result.summary);
    sumLabel->setWordWrap(true);
    sumLabel->setStyleSheet("color: #a6adc8; font-size: 11px;");
    fl->addWidget(sumLabel);

    if (!result.findings.isEmpty()) {
        auto* countLabel = new QLabel(
            QString("%1 finding(s) — click to expand").arg(result.findings.size()));
        countLabel->setStyleSheet(
            QString("color: %1; font-size: 11px; font-style: italic;").arg(color));
        fl->addWidget(countLabel);
    }

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(frame);

    setCursor(Qt::PointingHandCursor);
    setFixedHeight(110);
}

void MetricCard::mousePressEvent(QMouseEvent*)
{
    emit expandRequested(m_result);
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
CodeHealthFrame::CodeHealthFrame(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet("background: #1e1e2e; color: #cdd6f4;");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12);
    root->setSpacing(10);

    auto* topBar = new QHBoxLayout;
    topBar->setSpacing(8);

    auto* backBtn = new QPushButton("Back to Editor");
    backBtn->setFixedHeight(30);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; padding: 0 14px; font-size: 13px; }"
        "QPushButton:hover { background: #45475a; }");
    connect(backBtn, &QPushButton::clicked, this, &CodeHealthFrame::backToEditor);
    topBar->addWidget(backBtn);

    topBar->addStretch(1);

    auto* titleLabel = new QLabel("Code Health");
    titleLabel->setStyleSheet("color: #cdd6f4; font-size: 18px; font-weight: bold;");
    topBar->addWidget(titleLabel);

    topBar->addStretch(1);

    auto* helpBtn = new QPushButton("?");
    helpBtn->setFixedSize(36, 36);
    helpBtn->setCursor(Qt::PointingHandCursor);
    helpBtn->setToolTip("What is Code Health?");
    helpBtn->setStyleSheet(
        "QPushButton { background: #89b4fa; color: #1e1e2e; border: none;"
        " border-radius: 18px; font-size: 18px; font-weight: bold; }"
        "QPushButton:hover { background: #b4d0fb; }");
    connect(helpBtn, &QPushButton::clicked, this, &CodeHealthFrame::onHelpClicked);
    topBar->addWidget(helpBtn);

    root->addLayout(topBar);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #313244;");
    root->addWidget(sep);

    auto* gradeRow = new QHBoxLayout;
    gradeRow->setSpacing(14);

    m_gradeLabel = new QLabel("—");
    m_gradeLabel->setStyleSheet(
        "color: #6c7086; font-size: 64px; font-weight: bold;"
        " min-width: 80px; max-width: 80px;");
    m_gradeLabel->setAlignment(Qt::AlignCenter);
    gradeRow->addWidget(m_gradeLabel);

    auto* gradeRight = new QVBoxLayout;
    gradeRight->setSpacing(2);

    m_gradeCaption = new QLabel("Run a health check to score your code.");
    m_gradeCaption->setStyleSheet("color: #a6adc8; font-size: 13px;");
    m_gradeCaption->setWordWrap(true);
    gradeRight->addWidget(m_gradeCaption);

    m_statusLabel = new QLabel(QString());
    m_statusLabel->setStyleSheet("color: #6c7086; font-size: 11px;");
    gradeRight->addWidget(m_statusLabel);

    gradeRow->addLayout(gradeRight, 1);

    m_runBtn = new QPushButton("Run Health Check");
    m_runBtn->setFixedHeight(34);
    m_runBtn->setCursor(Qt::PointingHandCursor);
    m_runBtn->setStyleSheet(
        "QPushButton { background: #a6e3a1; color: #1e1e2e; border: none;"
        " border-radius: 4px; padding: 0 18px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: #c3f5bf; }"
        "QPushButton:disabled { background: #313244; color: #6c7086; }");
    connect(m_runBtn, &QPushButton::clicked, this, &CodeHealthFrame::onRunHealth);
    gradeRow->addWidget(m_runBtn);

    root->addLayout(gradeRow);

    auto* sep2 = new QFrame;
    sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet("color: #313244;");
    root->addWidget(sep2);

    m_cardGrid = new QWidget;
    m_cardGrid->setStyleSheet("background: transparent;");
    auto* emptyGridLayout = new QGridLayout(m_cardGrid);
    emptyGridLayout->setContentsMargins(0, 0, 0, 0);
    emptyGridLayout->setSpacing(10);
    root->addWidget(m_cardGrid);

    m_findingsTitle = new QLabel(QString());
    m_findingsTitle->setStyleSheet(
        "color: #cdd6f4; font-size: 13px; font-weight: bold; padding: 4px 0;");
    m_findingsTitle->hide();
    root->addWidget(m_findingsTitle);

    m_findingsScroll = new QScrollArea;
    m_findingsScroll->setWidgetResizable(true);
    m_findingsScroll->setVisible(false);
    m_findingsScroll->setStyleSheet(
        "QScrollArea { border: 1px solid #313244; background: #181825; border-radius: 4px; }"
        "QScrollBar:vertical { background: #1e1e2e; width: 8px; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background: #45475a; border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");
    m_findingsScroll->setMaximumHeight(260);
    root->addWidget(m_findingsScroll, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Public setter
// ─────────────────────────────────────────────────────────────────────────────
void CodeHealthFrame::setCode(const QString& code, const QString& language)
{
    m_code     = code;
    m_language = language;
}

// ─────────────────────────────────────────────────────────────────────────────
// Help dialog
// ─────────────────────────────────────────────────────────────────────────────
void CodeHealthFrame::onHelpClicked()
{
    // Build description from JSON metric names
    QJsonObject cfg = config();
    QString desc = "<b style='color:#cdd6f4;'>Code Health</b> is a report card for your code. "
        "It scores quality dimensions and gives you an overall grade.<br><br>";
    for (const QJsonValue& v : cfg["metrics"].toArray()) {
        QJsonObject m = v.toObject();
        desc += QString("<b style='color:#cdd6f4;'>%1</b> — %2<br>")
                    .arg(m["name"].toString(), m["description"].toString());
    }

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Code Health");
    dlg->setModal(true);
    dlg->setMinimumWidth(440);
    dlg->setStyleSheet("background: #1e1e2e;");

    auto* lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(22, 18, 22, 18);
    lay->setSpacing(14);

    auto* descLabel = new QLabel(desc);
    descLabel->setWordWrap(true);
    descLabel->setTextFormat(Qt::RichText);
    descLabel->setStyleSheet("QLabel { color: #cdd6f4; font-size: 13px; }");
    lay->addWidget(descLabel);

    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    auto* launchBtn = new QPushButton("Launch Tutorial");
    launchBtn->setStyleSheet(
        "QPushButton { background: #89b4fa; color: #1e1e2e; border: none;"
        " border-radius: 6px; padding: 0 18px; font-size: 13px; font-weight: bold; min-height: 30px; }"
        "QPushButton:hover { background: #b4d0fb; }");
    launchBtn->setCursor(Qt::PointingHandCursor);
    connect(launchBtn, &QPushButton::clicked, dlg, [dlg, this]() {
        dlg->accept();
        auto* tut = TutorialDialog::codeHealth(this);
        tut->exec();
        tut->deleteLater();
    });
    btnRow->addWidget(launchBtn);
    btnRow->addStretch();

    auto* closeBtn = new QPushButton("Close");
    closeBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 6px; padding: 0 14px; font-size: 13px; min-height: 30px; }"
        "QPushButton:hover { background: #45475a; }");
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    btnRow->addWidget(closeBtn);

    lay->addLayout(btnRow);
    dlg->exec();
    dlg->deleteLater();
}

// ─────────────────────────────────────────────────────────────────────────────
// Run
// ─────────────────────────────────────────────────────────────────────────────
void CodeHealthFrame::onRunHealth()
{
    if (m_code.trimmed().isEmpty()) {
        m_statusLabel->setText("No code loaded. Open a file in the editor first.");
        return;
    }

    m_runBtn->setEnabled(false);
    m_statusLabel->setText("Analysing…");

    QStringList lines = m_code.split('\n');
    HealthReport report = buildReport(lines, m_language);

    populateGrade(report);
    populateCards(report);

    m_findingsTitle->hide();
    m_findingsScroll->setVisible(false);

    m_statusLabel->setText(
        QString("%1 functions analysed across %2 lines.")
        .arg(report.complexity.findings.size())
        .arg(lines.size()));

    m_runBtn->setEnabled(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Grade + cards UI
// ─────────────────────────────────────────────────────────────────────────────
void CodeHealthFrame::populateGrade(const HealthReport& report)
{
    QString col = gradeColor(report.overallScore);
    m_gradeLabel->setText(report.grade);
    m_gradeLabel->setStyleSheet(
        QString("color: %1; font-size: 64px; font-weight: bold;"
                " min-width: 80px; max-width: 80px;").arg(col));

    // Grade descriptions from JSON
    QJsonObject gradeDescs = config()["grade_descriptions"].toObject();
    QString caption = gradeDescs[report.grade].toString();
    if (caption.isEmpty()) {
        switch (report.overallScore) {
        case HealthScore::Green:  caption = "Your code is in great shape."; break;
        case HealthScore::Yellow: caption = "Some areas need attention.";   break;
        case HealthScore::Red:    caption = "Several issues detected.";     break;
        }
    }
    m_gradeCaption->setText(caption);
    m_gradeCaption->setStyleSheet(QString("color: %1; font-size: 13px;").arg(col));
}

void CodeHealthFrame::populateCards(const HealthReport& report)
{
    QLayout* old = m_cardGrid->layout();
    if (old) {
        QLayoutItem* item;
        while ((item = old->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        delete old;
    }

    auto* grid = new QGridLayout(m_cardGrid);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(10);

    QList<MetricResult> metrics = {
        report.complexity, report.readability, report.duplication,
        report.deadCode, report.naming
    };

    for (int i = 0; i < metrics.size(); ++i) {
        auto* card = new MetricCard(metrics[i], m_cardGrid);
        connect(card, &MetricCard::expandRequested,
                this, &CodeHealthFrame::onCardExpanded);
        grid->addWidget(card, i / 3, i % 3);
    }

    int remaining = 3 - (metrics.size() % 3);
    if (remaining < 3) {
        for (int i = 0; i < remaining; ++i) {
            int pos = metrics.size() + i;
            auto* spacer = new QWidget;
            spacer->setFixedHeight(110);
            grid->addWidget(spacer, pos / 3, pos % 3);
        }
    }
}

void CodeHealthFrame::onCardExpanded(const MetricResult& result)
{
    showFindings(result);
}

void CodeHealthFrame::showFindings(const MetricResult& result)
{
    QString color = scoreColor(result.score);
    m_findingsTitle->setText(result.name + " — Details");
    m_findingsTitle->setStyleSheet(
        QString("color: %1; font-size: 13px; font-weight: bold; padding: 4px 0;").arg(color));
    m_findingsTitle->show();

    auto* container = new QWidget;
    container->setStyleSheet("background: #181825;");
    auto* vl = new QVBoxLayout(container);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    if (result.findings.isEmpty()) {
        auto* ok = new QLabel("No issues found.");
        ok->setAlignment(Qt::AlignCenter);
        ok->setStyleSheet("color: #a6e3a1; font-size: 13px; font-weight: bold; padding: 16px;");
        vl->addWidget(ok);
    } else {
        for (const auto& f : result.findings) {
            auto* row = new QFrame;
            row->setStyleSheet(QString(
                "QFrame { background: #1e1e2e; border-left: 3px solid %1;"
                " border-radius: 4px; }").arg(color));
            auto* rl = new QHBoxLayout(row);
            rl->setContentsMargins(10, 6, 10, 6);
            rl->setSpacing(10);

            auto* subjLabel = new QLabel(f.subject);
            subjLabel->setStyleSheet("color: #cdd6f4; font-size: 12px; font-weight: bold;");
            subjLabel->setMinimumWidth(120);
            subjLabel->setMaximumWidth(180);
            rl->addWidget(subjLabel);

            auto* detailLabel = new QLabel(f.detail);
            detailLabel->setWordWrap(true);
            detailLabel->setStyleSheet("color: #a6adc8; font-size: 12px;");
            rl->addWidget(detailLabel, 1);

            if (f.score > 0) {
                auto* scoreLabel = new QLabel(QString::number(f.score));
                scoreLabel->setStyleSheet(
                    QString("color: %1; font-size: 13px; font-weight: bold;").arg(color));
                scoreLabel->setFixedWidth(30);
                scoreLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                rl->addWidget(scoreLabel);
            }

            vl->addWidget(row);
        }
    }

    vl->addStretch(1);
    m_findingsScroll->setWidget(container);
    m_findingsScroll->setVisible(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Analysis: Complexity
// ─────────────────────────────────────────────────────────────────────────────
MetricResult CodeHealthFrame::analyzeComplexity(const QStringList& lines, const QString& lang)
{
    MetricResult result;
    result.name = "Complexity";
    QJsonObject cfg = metricConfig("complexity");

    QRegularExpression funcDef(
        R"(^\s*(?:(?:[\w\*\&:<>]+\s+)+)([\w~]+)\s*\([^;]*\)\s*(?:const\s*)?(?:noexcept\s*)?\{)",
        QRegularExpression::MultilineOption);
    QRegularExpression pyDef(R"(^\s*def\s+([\w]+)\s*\()");
    QRegularExpression decisionRx(R"(\b(if|else\s+if|elif|for|while|case|catch)\b|(\&\&|\|\|))");

    struct FuncEntry { QString name; int start; int end; };
    QList<FuncEntry> funcs;
    bool isPython = lang.toLower().contains("python") || lang.toLower() == "py";

    if (isPython) {
        for (int i = 0; i < lines.size(); ++i) {
            auto m = pyDef.match(lines[i]);
            if (!m.hasMatch()) continue;
            FuncEntry fe;
            fe.name  = m.captured(1);
            fe.start = i;
            int baseIndent = lines[i].indexOf("def");
            int end = lines.size() - 1;
            for (int j = i + 1; j < lines.size(); ++j) {
                QString tl = lines[j].trimmed();
                if (tl.isEmpty()) continue;
                int indent = lines[j].size() - lines[j].trimmed().size();
                if (indent <= baseIndent && (tl.startsWith("def ") || tl.startsWith("class "))) {
                    end = j - 1; break;
                }
            }
            fe.end = end;
            funcs.append(fe);
        }
    } else {
        for (int i = 0; i < lines.size(); ++i) {
            auto m = funcDef.match(lines[i]);
            if (!m.hasMatch()) continue;
            FuncEntry fe;
            fe.name  = m.captured(1);
            fe.start = i;
            int depth = 0, end = i;
            for (int j = i; j < lines.size(); ++j) {
                for (QChar ch : lines[j]) {
                    if (ch == '{') ++depth;
                    else if (ch == '}') { --depth; if (depth == 0) { end = j; goto done; } }
                }
            }
            done:
            fe.end = end;
            funcs.append(fe);
        }
    }

    if (funcs.isEmpty()) {
        FuncEntry fe; fe.name = "(file)"; fe.start = 0; fe.end = lines.size() - 1;
        funcs.append(fe);
    }

    const int threshold = cfg["finding_threshold"].toInt(10);
    QString findingTpl  = cfg["finding_template"].toString("Cyclomatic complexity %1 — consider splitting.");
    QJsonObject summaries = cfg["summaries"].toObject();

    QList<HealthFinding> findings;
    int maxScore = 1;

    for (const auto& fe : funcs) {
        int decisions = 0;
        for (int l = fe.start; l <= fe.end && l < lines.size(); ++l) {
            auto it = decisionRx.globalMatch(lines[l]);
            while (it.hasNext()) { it.next(); ++decisions; }
        }
        int cc = decisions + 1;
        if (cc > maxScore) maxScore = cc;
        if (cc >= threshold) {
            HealthFinding f;
            f.subject = fe.name;
            f.detail  = findingTpl.arg(cc);
            f.score   = cc;
            findings.append(f);
        }
    }

    std::sort(findings.begin(), findings.end(),
        [](const HealthFinding& a, const HealthFinding& b){ return a.score > b.score; });

    if (maxScore > 15 || (!findings.isEmpty() && findings[0].score > 15)) {
        result.score   = HealthScore::Red;
        result.summary = summaries["red"].toString().arg(maxScore);
    } else if (!findings.isEmpty()) {
        result.score   = HealthScore::Yellow;
        result.summary = summaries["yellow"].toString().arg(maxScore);
    } else {
        result.score   = HealthScore::Green;
        result.summary = summaries["green"].toString().arg(maxScore);
    }

    result.findings = findings;
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Analysis: Readability
// ─────────────────────────────────────────────────────────────────────────────
MetricResult CodeHealthFrame::analyzeReadability(const QStringList& lines, const QString& lang)
{
    MetricResult result;
    result.name = "Readability";
    QJsonObject cfg = metricConfig("readability");

    // Load thresholds from JSON
    int fnLenMax   = 30, paramMax = 5, nestMax = 4;
    QSet<QString> allowedSingle;
    for (const QJsonValue& cv : cfg["checks"].toArray()) {
        QJsonObject c = cv.toObject();
        if (c["id"] == "function_length")   fnLenMax   = c["threshold"].toInt(30);
        if (c["id"] == "parameter_count")   paramMax   = c["threshold"].toInt(5);
        if (c["id"] == "nesting_depth")     nestMax    = c["threshold"].toInt(4);
        if (c["id"] == "single_letter_vars")
            for (const QJsonValue& v : c["allowed"].toArray())
                allowedSingle.insert(v.toString());
    }
    if (allowedSingle.isEmpty()) allowedSingle = {"i","j","k","n","x","y"};

    QList<HealthFinding> findings;
    bool isPython = lang.toLower().contains("python") || lang.toLower() == "py";

    QRegularExpression singleLetterRx(
        R"(\b(?:int|float|double|char|auto|var|let|const)\s+([a-wz])\s*[=;,)])");
    QRegularExpression funcStart = isPython
        ? QRegularExpression(R"(^\s*def\s+([\w]+))")
        : QRegularExpression(R"(^\s*(?:(?:[\w\*\&:<>]+\s+)+)([\w~]+)\s*\([^;]*\)\s*(?:const\s*)?\{)");
    QRegularExpression paramRx(R"(\(([^)]*)\))");

    int maxNesting = 0;
    for (const auto& line : lines) {
        if (line.trimmed().isEmpty()) continue;
        int spaces = 0;
        for (QChar ch : line) {
            if (ch == ' ') ++spaces; else if (ch == '\t') spaces += 4; else break;
        }
        if (spaces / 4 > maxNesting) maxNesting = spaces / 4;
    }

    int funcStart_i = -1;
    QString funcName;
    for (int i = 0; i < lines.size(); ++i) {
        auto m = funcStart.match(lines[i]);
        if (m.hasMatch()) {
            if (funcStart_i >= 0) {
                int len = i - funcStart_i;
                if (len > fnLenMax) {
                    HealthFinding f;
                    f.subject = funcName;
                    f.detail  = QString("Function is %1 lines long (recommended max %2).").arg(len).arg(fnLenMax);
                    f.score   = len;
                    findings.append(f);
                }
            }
            funcStart_i = i;
            funcName = m.captured(1).isEmpty() ? m.captured(2) : m.captured(1);

            auto pm = paramRx.match(lines[i]);
            if (pm.hasMatch()) {
                QString paramStr = pm.captured(1).trimmed();
                if (!paramStr.isEmpty()) {
                    int paramCount = paramStr.split(',').size();
                    if (paramCount > paramMax) {
                        HealthFinding f;
                        f.subject = funcName;
                        f.detail  = QString("%1 parameters (recommended max %2). Consider grouping.")
                                        .arg(paramCount).arg(paramMax);
                        f.score   = paramCount;
                        findings.append(f);
                    }
                }
            }
        }

        auto slm = singleLetterRx.match(lines[i]);
        if (slm.hasMatch() && !allowedSingle.contains(slm.captured(1))) {
            HealthFinding f;
            f.subject = QString("'%1' at line %2").arg(slm.captured(1)).arg(i + 1);
            f.detail  = "Single-letter variable name makes code harder to understand.";
            findings.append(f);
        }
    }
    if (funcStart_i >= 0) {
        int len = lines.size() - funcStart_i;
        if (len > fnLenMax) {
            HealthFinding f;
            f.subject = funcName;
            f.detail  = QString("Function is %1 lines long (recommended max %2).").arg(len).arg(fnLenMax);
            f.score   = len;
            findings.append(f);
        }
    }

    if (maxNesting > nestMax) {
        HealthFinding f;
        f.subject = "Deep nesting";
        f.detail  = QString("Maximum nesting depth %1 (recommended max %2). Consider early returns.")
                        .arg(maxNesting).arg(nestMax);
        f.score   = maxNesting;
        findings.append(f);
    }

    int worstLen = 0;
    for (const auto& f : findings) if (f.score > worstLen) worstLen = f.score;

    QJsonObject summaries = cfg["summaries"].toObject();
    if (worstLen > 50 || maxNesting > nestMax + 1) {
        result.score   = HealthScore::Red;
        result.summary = summaries["red"].toString();
    } else if (!findings.isEmpty()) {
        result.score   = HealthScore::Yellow;
        result.summary = summaries["yellow"].toString();
    } else {
        result.score   = HealthScore::Green;
        result.summary = summaries["green"].toString();
    }

    result.findings = findings;
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Analysis: Duplication
// ─────────────────────────────────────────────────────────────────────────────
MetricResult CodeHealthFrame::analyzeDuplication(const QStringList& lines)
{
    MetricResult result;
    result.name = "Duplication";
    QJsonObject cfg = metricConfig("duplication");
    QJsonObject summaries = cfg["summaries"].toObject();

    QStringList meaningful;
    QList<int>  originalIndex;
    for (int i = 0; i < lines.size(); ++i) {
        QString t = lines[i].trimmed();
        if (t.isEmpty() || t == "{" || t == "}" || t == ":" || t.startsWith("//") || t.startsWith("#"))
            continue;
        meaningful.append(t);
        originalIndex.append(i);
    }

    const int BLOCK = 3;
    QMap<QString, QList<int>> blockMap;
    for (int i = 0; i + BLOCK <= meaningful.size(); ++i)
        blockMap[meaningful.mid(i, BLOCK).join('\n')].append(i);

    QSet<int> dupLines;
    QList<HealthFinding> findings;

    for (auto it = blockMap.begin(); it != blockMap.end(); ++it) {
        if (it.value().size() < 2) continue;
        for (int startIdx : it.value())
            for (int k = 0; k < BLOCK && startIdx + k < meaningful.size(); ++k)
                dupLines.insert(originalIndex[startIdx + k]);

        HealthFinding f;
        int firstLine = originalIndex[it.value()[0]];
        f.subject = QString("Line %1").arg(firstLine + 1);
        f.detail  = QString("Block repeated %1 times: \"%2…\"")
                        .arg(it.value().size())
                        .arg(meaningful[it.value()[0]].left(50));
        f.score   = it.value().size();

        bool already = false;
        for (const auto& ef : findings)
            if (ef.detail.contains(meaningful[it.value()[0]].left(20))) { already = true; break; }
        if (!already) findings.append(f);
    }

    double dupPct = meaningful.isEmpty() ? 0.0 : 100.0 * dupLines.size() / meaningful.size();

    if (dupPct > 10.0) {
        result.score   = HealthScore::Red;
        result.summary = QString("%1% of code is duplicated. Large copy-paste debt.").arg(int(dupPct));
    } else if (dupPct >= 3.0) {
        result.score   = HealthScore::Yellow;
        result.summary = summaries["yellow"].toString();
    } else {
        result.score   = HealthScore::Green;
        result.summary = summaries["green"].toString();
        findings.clear();
    }

    result.findings = findings;
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Analysis: Dead Code
// ─────────────────────────────────────────────────────────────────────────────
MetricResult CodeHealthFrame::analyzeDeadCode(const QStringList& lines, const QString& lang)
{
    MetricResult result;
    result.name = "Dead Code";
    QJsonObject cfg = metricConfig("dead_code");
    QJsonObject summaries = cfg["summaries"].toObject();

    QList<HealthFinding> findings;
    bool isPython = lang.toLower().contains("python") || lang.toLower() == "py";

    QRegularExpression earlyExit(R"(\b(return|break|continue|throw|exit|abort)\b)");
    for (int i = 0; i < lines.size() - 1; ++i) {
        QString t = lines[i].trimmed();
        if (t.startsWith("//") || t.startsWith("#")) continue;
        if (!earlyExit.match(t).hasMatch()) continue;
        for (int j = i + 1; j < lines.size(); ++j) {
            QString nt = lines[j].trimmed();
            if (nt.isEmpty()) continue;
            if (nt.startsWith("}") || nt.startsWith("else") || nt.startsWith("elif") ||
                nt.startsWith("except") || nt.startsWith("finally") ||
                nt.startsWith("case") || nt.startsWith("default")) break;
            HealthFinding f;
            f.subject = QString("Line %1").arg(j + 1);
            f.detail  = QString("Unreachable code after '%1' on line %2: \"%3\"")
                            .arg(earlyExit.match(t).captured(1)).arg(i + 1).arg(nt.left(50));
            findings.append(f);
            break;
        }
    }

    QRegularExpression varAssign = isPython
        ? QRegularExpression(R"(^\s*([\w]+)\s*=(?!=))")
        : QRegularExpression(R"(\b(?:int|float|double|char|bool|auto|string)\s+([\w]+)\s*=)");

    QMap<QString, int> assignLine;
    for (int i = 0; i < lines.size(); ++i) {
        auto m = varAssign.match(lines[i]);
        if (m.hasMatch()) {
            QString name = m.captured(1);
            if (name != "_" && !name.startsWith("__")) assignLine[name] = i;
        }
    }

    for (auto it = assignLine.begin(); it != assignLine.end(); ++it) {
        bool read = false;
        for (int i = 0; i < lines.size(); ++i) {
            if (i == it.value()) continue;
            if (lines[i].contains(QRegularExpression(QString("\\b%1\\b").arg(it.key())))) {
                read = true; break;
            }
        }
        if (!read) {
            HealthFinding f;
            f.subject = it.key();
            f.detail  = QString("Assigned at line %1 but never read.").arg(it.value() + 1);
            findings.append(f);
        }
    }

    if (findings.size() > 20) findings = findings.mid(0, 20);

    if (findings.size() > 3) {
        result.score   = HealthScore::Red;
        result.summary = summaries["red"].toString();
    } else if (!findings.isEmpty()) {
        result.score   = HealthScore::Yellow;
        result.summary = summaries["yellow"].toString();
    } else {
        result.score   = HealthScore::Green;
        result.summary = summaries["green"].toString();
    }

    result.findings = findings;
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Analysis: Naming
// ─────────────────────────────────────────────────────────────────────────────
MetricResult CodeHealthFrame::analyzeNaming(const QStringList& lines, const QString& lang)
{
    MetricResult result;
    result.name = "Naming";
    QJsonObject cfg = metricConfig("naming");
    QJsonObject summaries = cfg["summaries"].toObject();

    QList<HealthFinding> findings;
    bool isPython = lang.toLower().contains("python") || lang.toLower() == "py";

    QRegularExpression funcRx = isPython
        ? QRegularExpression(R"(^\s*def\s+([\w]+))")
        : QRegularExpression(R"(^\s*(?:(?:[\w\*\&:<>]+\s+)+)([\w~]+)\s*\()");

    int camelCount = 0, snakeCount = 0;

    for (int i = 0; i < lines.size(); ++i) {
        QString t = lines[i].trimmed();
        if (t.startsWith("//") || t.startsWith("#!") || t.startsWith("/*")) continue;

        auto fm = funcRx.match(lines[i]);
        if (fm.hasMatch()) {
            QString name = fm.captured(1).isEmpty() ? fm.captured(2) : fm.captured(1);
            if (!name.isEmpty() && !name.startsWith('~')) {
                if (isPython && name.contains(QRegularExpression("[A-Z]")) && !name[0].isUpper()) {
                    HealthFinding f;
                    f.subject = name;
                    f.detail  = QString("Function '%1' at line %2 uses camelCase. Python convention is snake_case.")
                                    .arg(name).arg(i + 1);
                    findings.append(f);
                }
                if (name.contains('_')) ++snakeCount;
                else if (name[0].isLower() && name.contains(QRegularExpression("[A-Z]"))) ++camelCount;
            }
        }

        if (isPython) {
            QRegularExpression pyVar(R"(^\s*([\w]+)\s*=(?!=))");
            auto vm = pyVar.match(lines[i]);
            if (vm.hasMatch()) {
                QString vname = vm.captured(1);
                if (vname.length() > 2 &&
                    vname.contains(QRegularExpression("[A-Z]")) &&
                    vname.contains(QRegularExpression("[a-z]")) &&
                    !vname[0].isUpper()) {
                    HealthFinding f;
                    f.subject = vname;
                    f.detail  = QString("Variable '%1' at line %2 looks like camelCase. Python convention is snake_case.")
                                    .arg(vname).arg(i + 1);
                    findings.append(f);
                }
            }
        }
    }

    if (camelCount > 2 && snakeCount > 2) {
        HealthFinding f;
        f.subject = "Mixed conventions";
        f.detail  = QString("File mixes camelCase (%1) and snake_case (%2). Pick one style.")
                        .arg(camelCount).arg(snakeCount);
        findings.prepend(f);
    }

    if (findings.size() > 15) findings = findings.mid(0, 15);
    double ratio = lines.isEmpty() ? 0 : double(findings.size()) / lines.size();

    if (findings.size() > 5 || ratio > 0.1) {
        result.score   = HealthScore::Red;
        result.summary = summaries["red"].toString();
    } else if (!findings.isEmpty()) {
        result.score   = HealthScore::Yellow;
        result.summary = summaries["yellow"].toString();
    } else {
        result.score   = HealthScore::Green;
        result.summary = summaries["green"].toString();
    }

    result.findings = findings;
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Build full report + grade
// ─────────────────────────────────────────────────────────────────────────────
HealthReport CodeHealthFrame::buildReport(const QStringList& lines, const QString& lang)
{
    HealthReport report;
    report.complexity   = analyzeComplexity(lines, lang);
    report.readability  = analyzeReadability(lines, lang);
    report.duplication  = analyzeDuplication(lines);
    report.deadCode     = analyzeDeadCode(lines, lang);
    report.naming       = analyzeNaming(lines, lang);

    int reds = 0, yellows = 0;
    for (HealthScore s : { report.complexity.score, report.readability.score,
                           report.duplication.score, report.deadCode.score, report.naming.score }) {
        if (s == HealthScore::Red)    ++reds;
        else if (s == HealthScore::Yellow) ++yellows;
    }

    if (reds == 0 && yellows == 0)        { report.grade = 'A'; report.overallScore = HealthScore::Green; }
    else if (reds == 0 && yellows <= 2)   { report.grade = 'B'; report.overallScore = HealthScore::Yellow; }
    else if (reds <= 1 && yellows <= 3)   { report.grade = 'C'; report.overallScore = HealthScore::Yellow; }
    else if (reds <= 2)                   { report.grade = 'D'; report.overallScore = HealthScore::Red; }
    else                                  { report.grade = 'F'; report.overallScore = HealthScore::Red; }

    return report;
}
