#include "core/securitytesting.h"
#include "core/sast.h"
#include "core/dast.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QMessageBox>
#include <QFont>
#include <QSizePolicy>

// ── Constructor ──────────────────────────────────────────────────────────────
SecurityTestingFrame::SecurityTestingFrame(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet("background: #1e1e2e; color: #cdd6f4;");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12);
    root->setSpacing(10);

    // ── Top bar: Back button + title + Help button ───────────────────────────
    auto* topBar = new QHBoxLayout;
    topBar->setSpacing(8);

    auto* backBtn = new QPushButton("Back to Editor");
    backBtn->setFixedHeight(30);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; padding: 0 14px; font-size: 13px; }"
        "QPushButton:hover { background: #45475a; }");
    connect(backBtn, &QPushButton::clicked, this, &SecurityTestingFrame::backToEditor);
    topBar->addWidget(backBtn);

    topBar->addStretch(1);

    auto* titleLabel = new QLabel("Security Testing");
    titleLabel->setStyleSheet("color: #cdd6f4; font-size: 18px; font-weight: bold;");
    topBar->addWidget(titleLabel);

    topBar->addStretch(1);

    // Large ? help button
    auto* helpBtn = new QPushButton("?");
    helpBtn->setFixedSize(36, 36);
    helpBtn->setCursor(Qt::PointingHandCursor);
    helpBtn->setToolTip("What are SAST and DAST?");
    helpBtn->setStyleSheet(
        "QPushButton { background: #89b4fa; color: #1e1e2e; border: none;"
        " border-radius: 18px; font-size: 18px; font-weight: bold; }"
        "QPushButton:hover { background: #b4d0fb; }");
    connect(helpBtn, &QPushButton::clicked, this, &SecurityTestingFrame::onHelpClicked);
    topBar->addWidget(helpBtn);

    root->addLayout(topBar);

    // ── Separator ────────────────────────────────────────────────────────────
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #313244;");
    root->addWidget(sep);

    // ── Tabs ─────────────────────────────────────────────────────────────────
    m_tabs = new QTabWidget;
    m_tabs->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #313244; background: #181825; border-radius: 4px; }"
        "QTabBar::tab { background: #313244; color: #a6adc8; padding: 6px 18px;"
        " border-radius: 4px 4px 0 0; margin-right: 2px; font-size: 13px; }"
        "QTabBar::tab:selected { background: #45475a; color: #cdd6f4; }");

    // ── SAST Tab ─────────────────────────────────────────────────────────────
    m_sastTab = new QWidget;
    m_sastTab->setStyleSheet("background: #181825;");
    auto* sastLayout = new QVBoxLayout(m_sastTab);
    sastLayout->setContentsMargins(12, 12, 12, 12);
    sastLayout->setSpacing(8);

    // SAST header row
    auto* sastHeader = new QHBoxLayout;
    m_runSastBtn = new QPushButton("Run Static Analysis");
    m_runSastBtn->setFixedHeight(32);
    m_runSastBtn->setCursor(Qt::PointingHandCursor);
    m_runSastBtn->setStyleSheet(
        "QPushButton { background: #a6e3a1; color: #1e1e2e; border: none;"
        " border-radius: 4px; padding: 0 18px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: #c3f5bf; }"
        "QPushButton:disabled { background: #313244; color: #6c7086; }");
    connect(m_runSastBtn, &QPushButton::clicked, this, &SecurityTestingFrame::onRunSast);
    sastHeader->addWidget(m_runSastBtn);

    m_sastStatus = new QLabel("Press Run to scan your code for security issues.");
    m_sastStatus->setStyleSheet("color: #a6adc8; font-size: 12px; padding-left: 10px;");
    sastHeader->addWidget(m_sastStatus, 1);
    sastLayout->addLayout(sastHeader);

    m_sastScroll = new QScrollArea;
    m_sastScroll->setWidgetResizable(true);
    m_sastScroll->setStyleSheet(
        "QScrollArea { border: none; background: #181825; }"
        "QScrollBar:vertical { background: #1e1e2e; width: 8px; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background: #45475a; border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");
    auto* sastPlaceholder = new QLabel("No analysis run yet.");
    sastPlaceholder->setAlignment(Qt::AlignCenter);
    sastPlaceholder->setStyleSheet("color: #6c7086; font-size: 13px;");
    m_sastScroll->setWidget(sastPlaceholder);
    sastLayout->addWidget(m_sastScroll, 1);

    m_tabs->addTab(m_sastTab, "Static Analysis (SAST)");

    // ── DAST Tab ─────────────────────────────────────────────────────────────
    m_dastTab = new QWidget;
    m_dastTab->setStyleSheet("background: #181825;");
    auto* dastLayout = new QVBoxLayout(m_dastTab);
    dastLayout->setContentsMargins(12, 12, 12, 12);
    dastLayout->setSpacing(8);

    auto* dastHeader = new QHBoxLayout;
    m_runDastBtn = new QPushButton("Run Dynamic Tests");
    m_runDastBtn->setFixedHeight(32);
    m_runDastBtn->setCursor(Qt::PointingHandCursor);
    m_runDastBtn->setStyleSheet(
        "QPushButton { background: #89b4fa; color: #1e1e2e; border: none;"
        " border-radius: 4px; padding: 0 18px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: #b4d0fb; }"
        "QPushButton:disabled { background: #313244; color: #6c7086; }");
    connect(m_runDastBtn, &QPushButton::clicked, this, &SecurityTestingFrame::onRunDast);
    dastHeader->addWidget(m_runDastBtn);

    m_dastStatus = new QLabel("Requires a compiled executable. Build your program first.");
    m_dastStatus->setStyleSheet("color: #a6adc8; font-size: 12px; padding-left: 10px;");
    dastHeader->addWidget(m_dastStatus, 1);
    dastLayout->addLayout(dastHeader);

    m_dastScroll = new QScrollArea;
    m_dastScroll->setWidgetResizable(true);
    m_dastScroll->setStyleSheet(
        "QScrollArea { border: none; background: #181825; }"
        "QScrollBar:vertical { background: #1e1e2e; width: 8px; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background: #45475a; border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");
    auto* dastPlaceholder = new QLabel("No tests run yet.");
    dastPlaceholder->setAlignment(Qt::AlignCenter);
    dastPlaceholder->setStyleSheet("color: #6c7086; font-size: 13px;");
    m_dastScroll->setWidget(dastPlaceholder);
    dastLayout->addWidget(m_dastScroll, 1);

    m_tabs->addTab(m_dastTab, "Dynamic Testing (DAST)");

    root->addWidget(m_tabs, 1);
}

// ── Public setters ───────────────────────────────────────────────────────────
void SecurityTestingFrame::setCode(const QString& code, const QString& language)
{
    m_code     = code;
    m_language = language;
}

void SecurityTestingFrame::setExePath(const QString& exePath)
{
    m_exePath = exePath;
    m_runDastBtn->setEnabled(!exePath.isEmpty());
    if (!exePath.isEmpty())
        m_dastStatus->setText("Executable ready. Press Run to test with attack inputs.");
}

// ── Help dialog ──────────────────────────────────────────────────────────────
void SecurityTestingFrame::onHelpClicked()
{
    auto* dlg = new QMessageBox(this);
    dlg->setWindowTitle("What are SAST and DAST?");
    dlg->setIcon(QMessageBox::Information);
    dlg->setText(
        "<b>Static Analysis (SAST)</b><br>"
        "Scans your source code <i>without running it</i>. Looks for security vulnerabilities, "
        "dangerous patterns, and common mistakes that hackers exploit. Think of it as a security "
        "expert reading your code line by line.<br><br>"
        "<b>Dynamic Testing (DAST)</b><br>"
        "Runs your program and watches what happens. Tests it with bad inputs, unexpected data, "
        "and attack patterns to see if it breaks or leaks information. Think of it as someone "
        "trying to hack your program while it runs."
    );
    dlg->exec();
}

// ── Run SAST ─────────────────────────────────────────────────────────────────
void SecurityTestingFrame::onRunSast()
{
    if (m_code.trimmed().isEmpty()) {
        m_sastStatus->setText("No code loaded. Open a file in the editor first.");
        return;
    }

    m_runSastBtn->setEnabled(false);
    m_sastStatus->setText("Scanning...");

    StaticAnalyzer analyzer;
    QList<SecurityFinding> findings = analyzer.analyzeCode(m_code, m_language);

    populateResults(m_sastScroll, findings);

    if (findings.isEmpty()) {
        m_sastStatus->setText("No issues found.");
    } else {
        int critical = 0, high = 0, medium = 0, low = 0;
        for (const auto& f : findings) {
            switch (f.severity) {
            case SecurityFinding::CRITICAL: ++critical; break;
            case SecurityFinding::HIGH:     ++high;     break;
            case SecurityFinding::MEDIUM:   ++medium;   break;
            case SecurityFinding::LOW:      ++low;      break;
            }
        }
        m_sastStatus->setText(QString("%1 issue(s) found: %2 critical, %3 high, %4 medium, %5 low")
            .arg(findings.size()).arg(critical).arg(high).arg(medium).arg(low));
    }

    m_runSastBtn->setEnabled(true);
}

// ── Run DAST ─────────────────────────────────────────────────────────────────
void SecurityTestingFrame::onRunDast()
{
    if (m_exePath.isEmpty()) {
        m_dastStatus->setText("No executable set. Build your program from the editor first.");
        return;
    }

    m_runDastBtn->setEnabled(false);
    m_dastStatus->setText("Running attack inputs...");

    DynamicTester tester;
    QList<SecurityFinding> findings = tester.testProgram(m_exePath, m_language);

    populateResults(m_dastScroll, findings);

    if (findings.isEmpty()) {
        m_dastStatus->setText("All tests passed — no crashes or leaks detected.");
    } else {
        m_dastStatus->setText(QString("%1 issue(s) detected during dynamic testing.").arg(findings.size()));
    }

    m_runDastBtn->setEnabled(true);
}

// ── Populate results area ────────────────────────────────────────────────────
void SecurityTestingFrame::populateResults(QScrollArea* area,
                                           const QList<SecurityFinding>& findings)
{
    auto* container = new QWidget;
    container->setStyleSheet("background: #181825;");
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    if (findings.isEmpty()) {
        auto* noIssues = new QLabel("No security issues found.");
        noIssues->setAlignment(Qt::AlignCenter);
        noIssues->setStyleSheet(
            "color: #a6e3a1; font-size: 14px; font-weight: bold; padding: 30px;");
        layout->addWidget(noIssues);
    } else {
        // Sort: CRITICAL first
        QList<SecurityFinding> sorted = findings;
        std::sort(sorted.begin(), sorted.end(),
            [](const SecurityFinding& a, const SecurityFinding& b) {
                return a.severity > b.severity;
            });

        for (const auto& f : sorted) {
            layout->addWidget(buildCard(f));
        }
    }

    layout->addStretch(1);
    area->setWidget(container);
}

// ── Card builder ─────────────────────────────────────────────────────────────
QWidget* SecurityTestingFrame::buildCard(const SecurityFinding& f, bool showJump)
{
    QString color = severityColor(f.severity);
    QString label = severityLabel(f.severity);

    auto* card = new QFrame;
    card->setStyleSheet(QString(
        "QFrame { background: #1e1e2e; border: 1px solid %1;"
        " border-left: 4px solid %1; border-radius: 6px; }").arg(color));

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(12, 10, 12, 10);
    cardLayout->setSpacing(4);

    // ── Header row: severity badge + title + line number ────────────────────
    auto* headerRow = new QHBoxLayout;
    headerRow->setSpacing(8);

    auto* sevBadge = new QLabel(label);
    sevBadge->setStyleSheet(QString(
        "QLabel { background: %1; color: #1e1e2e; font-size: 11px; font-weight: bold;"
        " border-radius: 3px; padding: 2px 7px; }").arg(color));
    sevBadge->setFixedHeight(20);
    headerRow->addWidget(sevBadge);

    auto* titleLabel = new QLabel(f.title);
    titleLabel->setStyleSheet("color: #cdd6f4; font-size: 14px; font-weight: bold;");
    headerRow->addWidget(titleLabel, 1);

    if (f.lineNumber > 0 && showJump) {
        auto* lineBtn = new QPushButton(QString("Line %1").arg(f.lineNumber));
        lineBtn->setFixedHeight(22);
        lineBtn->setCursor(Qt::PointingHandCursor);
        lineBtn->setStyleSheet(
            "QPushButton { background: #313244; color: #89b4fa; border: 1px solid #45475a;"
            " border-radius: 3px; padding: 0 8px; font-size: 12px; }"
            "QPushButton:hover { background: #45475a; color: #b4d0fb; }");
        int ln = f.lineNumber;
        connect(lineBtn, &QPushButton::clicked, this, [this, ln]() {
            emit jumpToLine(ln);
        });
        headerRow->addWidget(lineBtn);
    } else if (f.lineNumber < 0 && !f.matchedText.isEmpty()) {
        auto* inputLabel = new QLabel(f.matchedText.left(50));
        inputLabel->setStyleSheet("color: #6c7086; font-size: 11px;");
        headerRow->addWidget(inputLabel);
    }

    cardLayout->addLayout(headerRow);

    // ── Separator ────────────────────────────────────────────────────────────
    auto* sepLine = new QFrame;
    sepLine->setFrameShape(QFrame::HLine);
    sepLine->setStyleSheet(QString("color: %1;").arg(color));
    cardLayout->addWidget(sepLine);

    // ── Description ──────────────────────────────────────────────────────────
    auto* descLabel = new QLabel(f.description);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("color: #cdd6f4; font-size: 13px;");
    cardLayout->addWidget(descLabel);

    // ── Fix suggestion ────────────────────────────────────────────────────────
    if (!f.fixSuggestion.isEmpty()) {
        auto* fixRow = new QHBoxLayout;
        auto* fixIcon = new QLabel("Fix:");
        fixIcon->setStyleSheet(QString(
            "color: %1; font-size: 12px; font-weight: bold;").arg(color));
        fixIcon->setFixedWidth(28);
        fixRow->addWidget(fixIcon);

        auto* fixLabel = new QLabel(f.fixSuggestion);
        fixLabel->setWordWrap(true);
        fixLabel->setStyleSheet("color: #a6adc8; font-size: 12px;");
        fixRow->addWidget(fixLabel, 1);
        cardLayout->addLayout(fixRow);
    }

    return card;
}

// ── Severity helpers ─────────────────────────────────────────────────────────
QString SecurityTestingFrame::severityColor(SecurityFinding::Severity sev)
{
    switch (sev) {
    case SecurityFinding::CRITICAL: return "#f38ba8";  // red
    case SecurityFinding::HIGH:     return "#fab387";  // orange
    case SecurityFinding::MEDIUM:   return "#f9e2af";  // yellow
    case SecurityFinding::LOW:      return "#89b4fa";  // blue
    }
    return "#89b4fa";
}

QString SecurityTestingFrame::severityLabel(SecurityFinding::Severity sev)
{
    switch (sev) {
    case SecurityFinding::CRITICAL: return "CRITICAL";
    case SecurityFinding::HIGH:     return "HIGH";
    case SecurityFinding::MEDIUM:   return "MEDIUM";
    case SecurityFinding::LOW:      return "LOW";
    }
    return "LOW";
}
