#include "core/securitytesting.h"
#include "core/sast.h"
#include "core/dast.h"
#include "core/tutorial.h"
#include "core/theme.h"

using C = Theme::Colors;

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QMessageBox>
#include <QSizePolicy>
#include <QDesktopServices>
#include <QUrl>
#include <QDialog>

// ── Constructor ──────────────────────────────────────────────────────────────
SecurityTestingFrame::SecurityTestingFrame(QWidget* parent)
    : AnalysisFrame("Security Testing", parent)
{
    // ── Tabs ─────────────────────────────────────────────────────────────────
    m_tabs = new QTabWidget;
    m_tabs->setStyleSheet(
        QString("QTabWidget::pane { border: 1px solid %1; background: %2; border-radius: 4px; }"
        "QTabBar::tab { background: %1; color: %3; padding: 6px 18px;"
        " border-radius: 4px 4px 0 0; margin-right: 2px; font-size: 13px; }"
        "QTabBar::tab:selected { background: %4; color: %5; }")
        .arg(C::bg2(), C::bg(), C::fg2(), C::border(), C::fg()));

    // ── SAST Tab ─────────────────────────────────────────────────────────────
    m_sastTab = new QWidget;
    m_sastTab->setStyleSheet(QString("background: %1;").arg(C::bg()));
    auto* sastLayout = new QVBoxLayout(m_sastTab);
    sastLayout->setContentsMargins(12, 12, 12, 12);
    sastLayout->setSpacing(8);

    auto* sastHeader = new QHBoxLayout;
    m_runSastBtn = new QPushButton("Run Static Analysis");
    m_runSastBtn->setFixedHeight(32);
    m_runSastBtn->setCursor(Qt::PointingHandCursor);
    m_runSastBtn->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: none;"
        " border-radius: 4px; padding: 0 18px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: %1; }"
        "QPushButton:disabled { background: %3; color: %4; }")
        .arg(C::green(), C::bg(), C::bg2(), C::fg3()));
    connect(m_runSastBtn, &QPushButton::clicked, this, &SecurityTestingFrame::onRunSast);
    sastHeader->addWidget(m_runSastBtn);

    m_sastStatus = new QLabel("Press Run to scan your code for security issues.");
    m_sastStatus->setStyleSheet(QString("color: %1; font-size: 12px; padding-left: 10px;").arg(C::fg2()));
    sastHeader->addWidget(m_sastStatus, 1);
    sastLayout->addLayout(sastHeader);

    m_sastScroll = new QScrollArea;
    m_sastScroll->setWidgetResizable(true);
    m_sastScroll->setStyleSheet(
        QString("QScrollArea { border: none; background: %1; }"
        "QScrollBar:vertical { background: %2; width: 8px; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background: %3; border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(C::bg(), C::bg(), C::border()));
    auto* sastPlaceholder = new QLabel("No analysis run yet.");
    sastPlaceholder->setAlignment(Qt::AlignCenter);
    sastPlaceholder->setStyleSheet(QString("color: %1; font-size: 13px;").arg(C::fg3()));
    m_sastScroll->setWidget(sastPlaceholder);
    sastLayout->addWidget(m_sastScroll, 1);

    m_tabs->addTab(m_sastTab, "Static Analysis (SAST)");

    // ── DAST Tab ─────────────────────────────────────────────────────────────
    m_dastTab = new QWidget;
    m_dastTab->setStyleSheet(QString("background: %1;").arg(C::bg()));
    auto* dastLayout = new QVBoxLayout(m_dastTab);
    dastLayout->setContentsMargins(12, 12, 12, 12);
    dastLayout->setSpacing(8);

    auto* dastHeader = new QHBoxLayout;
    m_runDastBtn = new QPushButton("Run Dynamic Tests");
    m_runDastBtn->setFixedHeight(32);
    m_runDastBtn->setCursor(Qt::PointingHandCursor);
    m_runDastBtn->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: none;"
        " border-radius: 4px; padding: 0 18px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: %1; }"
        "QPushButton:disabled { background: %3; color: %4; }")
        .arg(C::accent(), C::bg(), C::bg2(), C::fg3()));
    connect(m_runDastBtn, &QPushButton::clicked, this, &SecurityTestingFrame::onRunDast);
    dastHeader->addWidget(m_runDastBtn);

    m_dastStatus = new QLabel("Requires a compiled executable. Build your program first.");
    m_dastStatus->setStyleSheet(QString("color: %1; font-size: 12px; padding-left: 10px;").arg(C::fg2()));
    dastHeader->addWidget(m_dastStatus, 1);
    dastLayout->addLayout(dastHeader);

    m_dastScroll = new QScrollArea;
    m_dastScroll->setWidgetResizable(true);
    m_dastScroll->setStyleSheet(
        QString("QScrollArea { border: none; background: %1; }"
        "QScrollBar:vertical { background: %2; width: 8px; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background: %3; border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(C::bg(), C::bg(), C::border()));
    auto* dastPlaceholder = new QLabel("No tests run yet.");
    dastPlaceholder->setAlignment(Qt::AlignCenter);
    dastPlaceholder->setStyleSheet(QString("color: %1; font-size: 13px;").arg(C::fg3()));
    m_dastScroll->setWidget(dastPlaceholder);
    dastLayout->addWidget(m_dastScroll, 1);

    m_tabs->addTab(m_dastTab, "Dynamic Testing (DAST)");

    // ── CVE Auto-Check Tab ────────────────────────────────────────────────────
    m_cveTab = new QWidget;
    m_cveTab->setStyleSheet(QString("background: %1;").arg(C::bg()));
    auto* cveLayout = new QVBoxLayout(m_cveTab);
    cveLayout->setContentsMargins(16, 16, 16, 16);
    cveLayout->setSpacing(12);

    // Section title
    auto* cveTitleLbl = new QLabel("CVE Vulnerability Monitoring");
    cveTitleLbl->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;"
                               " background: transparent;").arg(C::fg()));
    cveLayout->addWidget(cveTitleLbl);

    auto* cveDescLbl = new QLabel(
        "Runs pip-audit in the background to check your Python dependencies for "
        "known CVEs (Common Vulnerabilities and Exposures). Only runs when enabled.");
    cveDescLbl->setWordWrap(true);
    cveDescLbl->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(C::fg2()));
    cveLayout->addWidget(cveDescLbl);

    // Separator
    auto* cveSep = new QFrame;
    cveSep->setFrameShape(QFrame::HLine);
    cveSep->setStyleSheet(QString("color: %1; background: %1;").arg(C::bg2()));
    cveLayout->addWidget(cveSep);

    // Enable toggle
    m_cveEnabledChk = new QCheckBox("Enable Auto CVE Check");
    m_cveEnabledChk->setStyleSheet(
        QString("QCheckBox { color: %1; font-size: 13px; background: transparent; }"
        "QCheckBox::indicator { width: 16px; height: 16px; border-radius: 3px;"
        " border: 1px solid %2; background: %3; }"
        "QCheckBox::indicator:checked { background: %4; border-color: %4; }")
        .arg(C::fg(), C::border(), C::bg2(), C::accent()));
    m_cveEnabledChk->setChecked(CVEMonitor::loadEnabled());
    connect(m_cveEnabledChk, &QCheckBox::toggled, this, &SecurityTestingFrame::onCVEToggled);
    cveLayout->addWidget(m_cveEnabledChk);

    // Frequency row
    auto* freqRow = new QHBoxLayout;
    freqRow->setSpacing(10);
    auto* freqLabel = new QLabel("Check Frequency:");
    freqLabel->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(C::fg()));
    freqRow->addWidget(freqLabel);

    m_cveFreqCombo = new QComboBox;
    m_cveFreqCombo->addItem("Every 5 minutes",  static_cast<int>(CVEMonitor::Every5Min));
    m_cveFreqCombo->addItem("Every 30 minutes", static_cast<int>(CVEMonitor::Every30Min));
    m_cveFreqCombo->addItem("Hourly",            static_cast<int>(CVEMonitor::Hourly));
    m_cveFreqCombo->addItem("Daily (Default)",   static_cast<int>(CVEMonitor::Daily));
    m_cveFreqCombo->addItem("Weekly",            static_cast<int>(CVEMonitor::Weekly));
    m_cveFreqCombo->setStyleSheet(
        QString("QComboBox { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 4px; padding: 4px 10px; font-size: 12px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: %1; color: %2;"
        " selection-background-color: %3; }")
        .arg(C::bg2(), C::fg(), C::border()));
    // Select saved frequency
    {
        int savedMs = CVEMonitor::loadFrequencyMs();
        for (int i = 0; i < m_cveFreqCombo->count(); ++i) {
            if (m_cveFreqCombo->itemData(i).toInt() == savedMs) {
                m_cveFreqCombo->setCurrentIndex(i);
                break;
            }
        }
        // Default to Daily if no match
        if (m_cveFreqCombo->currentIndex() < 0) m_cveFreqCombo->setCurrentIndex(3);
    }
    connect(m_cveFreqCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SecurityTestingFrame::onCVEFrequencyChanged);
    freqRow->addWidget(m_cveFreqCombo);
    freqRow->addStretch();
    cveLayout->addLayout(freqRow);

    // Manual run button
    m_cveRunNowBtn = new QPushButton("Check Now");
    m_cveRunNowBtn->setFixedHeight(32);
    m_cveRunNowBtn->setFixedWidth(120);
    m_cveRunNowBtn->setCursor(Qt::PointingHandCursor);
    m_cveRunNowBtn->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: none;"
        " border-radius: 4px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background: %1; }"
        "QPushButton:disabled { background: %3; color: %4; }")
        .arg(C::red(), C::bg(), C::bg2(), C::fg3()));
    connect(m_cveRunNowBtn, &QPushButton::clicked, this, &SecurityTestingFrame::onRunCVENow);
    cveLayout->addWidget(m_cveRunNowBtn);

    // Status label
    m_cveStatusLabel = new QLabel("Not checked yet.");
    m_cveStatusLabel->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(C::fg2()));
    m_cveStatusLabel->setWordWrap(true);
    cveLayout->addWidget(m_cveStatusLabel);

    cveLayout->addStretch();

    // Note about pip-audit requirement
    auto* noteLabel = new QLabel(
        "Requires pip-audit to be installed: pip install pip-audit\n"
        "Results also appear in the Dependency Analysis frame.");
    noteLabel->setWordWrap(true);
    noteLabel->setStyleSheet(QString("color: %1; font-size: 11px; font-style: italic;"
                             " background: transparent;").arg(C::fg3()));
    cveLayout->addWidget(noteLabel);

    m_tabs->addTab(m_cveTab, "CVE Auto-Check");

    addResultWidget(m_tabs);
}

// ── Public setters ───────────────────────────────────────────────────────────
void SecurityTestingFrame::setCode(const QString& code, const QString& language,
                                   const QString& filePath)
{
    AnalysisFrame::setCode(code, language);
    m_filePath = filePath;
}

void SecurityTestingFrame::setExePath(const QString& exePath)
{
    m_exePath = exePath;
    m_runDastBtn->setEnabled(!exePath.isEmpty());
    if (!exePath.isEmpty())
        m_dastStatus->setText("Executable ready. Press Run to test with attack inputs.");
}

// ── Help dialog ──────────────────────────────────────────────────────────────
void SecurityTestingFrame::showHelp()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Security Testing");
    dlg->setModal(true);
    dlg->setMinimumWidth(420);
    dlg->setStyleSheet(QString("background: %1;").arg(C::bg()));

    auto* lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(22, 18, 22, 18);
    lay->setSpacing(14);

    auto* desc = new QLabel(
        "Security testing checks your code for weaknesses that hackers could exploit to "
        "steal data, crash your program, or take control of your computer. "
        "SAST scans your source code without running it. "
        "DAST runs your program and attacks it with bad inputs.");
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; }").arg(C::fg()));
    lay->addWidget(desc);

    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    auto* launchBtn = new QPushButton("Launch Tutorial");
    launchBtn->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: none;"
        " border-radius: 6px; padding: 0 18px; font-size: 13px; font-weight: bold; min-height: 30px; }"
        "QPushButton:hover { background: %1; }")
        .arg(C::accent(), C::bg()));
    launchBtn->setCursor(Qt::PointingHandCursor);
    connect(launchBtn, &QPushButton::clicked, dlg, [dlg, this]() {
        dlg->accept();
        auto* tut = TutorialDialog::security(this);
        tut->exec();
        tut->deleteLater();
    });
    btnRow->addWidget(launchBtn);

    btnRow->addStretch();

    auto* closeBtn = new QPushButton("Close");
    closeBtn->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 6px; padding: 0 14px; font-size: 13px; min-height: 30px; }"
        "QPushButton:hover { background: %3; }")
        .arg(C::bg2(), C::fg(), C::border()));
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    btnRow->addWidget(closeBtn);

    lay->addLayout(btnRow);
    dlg->exec();
    dlg->deleteLater();
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
    QList<SecurityFinding> findings = analyzer.analyzeCode(m_code, m_language, m_filePath);

    populateResults(m_sastScroll, findings,
                    analyzer.attributionName(), analyzer.attributionUrl());

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
        m_sastStatus->setText(
            QString("%1 issue(s) found: %2 critical, %3 high, %4 medium, %5 low")
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

    populateResults(m_dastScroll, findings, QString(), QString());

    if (findings.isEmpty()) {
        m_dastStatus->setText("All tests passed — no crashes or leaks detected.");
    } else {
        m_dastStatus->setText(
            QString("%1 issue(s) detected during dynamic testing.").arg(findings.size()));
    }

    m_runDastBtn->setEnabled(true);
}

// ── Populate results ─────────────────────────────────────────────────────────
void SecurityTestingFrame::populateResults(QScrollArea* area,
                                           const QList<SecurityFinding>& findings,
                                           const QString& attributionName,
                                           const QString& attributionUrl)
{
    auto* container = new QWidget;
    container->setStyleSheet(QString("background: %1;").arg(C::bg()));
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    if (findings.isEmpty()) {
        auto* noIssues = new QLabel("No security issues found.");
        noIssues->setAlignment(Qt::AlignCenter);
        noIssues->setStyleSheet(
            QString("color: %1; font-size: 14px; font-weight: bold; padding: 30px;").arg(C::green()));
        layout->addWidget(noIssues);
    } else {
        QList<SecurityFinding> sorted = findings;
        std::sort(sorted.begin(), sorted.end(),
            [](const SecurityFinding& a, const SecurityFinding& b) {
                return a.severity > b.severity;
            });
        for (const auto& f : sorted)
            layout->addWidget(buildCard(f));
    }

    layout->addStretch(1);

    // ── Attribution link (only shown when an external tool ran) ──────────────
    if (!attributionName.isEmpty() && !attributionUrl.isEmpty()) {
        auto* attrRow = new QHBoxLayout;
        attrRow->addStretch(1);

        auto* attrLabel = new QLabel("Analysis powered by ");
        attrLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(C::fg3()));
        attrRow->addWidget(attrLabel);

        auto* attrLink = new QPushButton(attributionName);
        attrLink->setFlat(true);
        attrLink->setCursor(Qt::PointingHandCursor);
        attrLink->setStyleSheet(
            QString("QPushButton { background: none; color: %1; font-size: 11px;"
            " border: none; padding: 0; text-decoration: underline; }"
            "QPushButton:hover { color: %1; }")
            .arg(C::accent()));
        QString url = attributionUrl;
        connect(attrLink, &QPushButton::clicked, this, [url]() {
            QDesktopServices::openUrl(QUrl(url));
        });
        attrRow->addWidget(attrLink);
        attrRow->addStretch(1);

        layout->addLayout(attrRow);
    }

    area->setWidget(container);
}

// ── Card builder ─────────────────────────────────────────────────────────────
QWidget* SecurityTestingFrame::buildCard(const SecurityFinding& f)
{
    QString color = severityColor(f.severity);
    QString label = severityLabel(f.severity);

    auto* card = new QFrame;
    card->setStyleSheet(QString(
        "QFrame { background: %2; border: 1px solid %1;"
        " border-left: 4px solid %1; border-radius: 6px; }").arg(color, C::bg()));

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(12, 10, 12, 10);
    cardLayout->setSpacing(6);

    // ── Header: severity badge + title + line button ─────────────────────────
    auto* headerRow = new QHBoxLayout;
    headerRow->setSpacing(8);

    auto* sevBadge = new QLabel(label);
    sevBadge->setStyleSheet(QString(
        "QLabel { background: %1; color: %2; font-size: 11px; font-weight: bold;"
        " border-radius: 3px; padding: 2px 7px; }").arg(color, C::bg()));
    sevBadge->setFixedHeight(20);
    headerRow->addWidget(sevBadge);

    auto* titleLabel = new QLabel(f.title);
    titleLabel->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;").arg(C::fg()));
    headerRow->addWidget(titleLabel, 1);

    if (f.lineNumber > 0) {
        auto* lineBtn = new QPushButton(QString("Line %1").arg(f.lineNumber));
        lineBtn->setFixedHeight(22);
        lineBtn->setCursor(Qt::PointingHandCursor);
        lineBtn->setStyleSheet(
            QString("QPushButton { background: %1; color: %2; border: 1px solid %3;"
            " border-radius: 3px; padding: 0 8px; font-size: 12px; }"
            "QPushButton:hover { background: %3; color: %2; }")
            .arg(C::bg2(), C::accent(), C::border()));
        int ln = f.lineNumber;
        connect(lineBtn, &QPushButton::clicked, this, [this, ln]() {
            emit jumpToLine(QString(), ln);
        });
        headerRow->addWidget(lineBtn);
    } else if (!f.matchedText.isEmpty()) {
        auto* inputLabel = new QLabel(f.matchedText.left(50));
        inputLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(C::fg3()));
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
    descLabel->setStyleSheet(QString("color: %1; font-size: 13px;").arg(C::fg()));
    cardLayout->addWidget(descLabel);

    // ── Why This Matters ─────────────────────────────────────────────────────
    if (!f.whyItMatters.isEmpty()) {
        auto* whyRow = new QHBoxLayout;
        whyRow->setSpacing(6);

        auto* whyIcon = new QLabel("Why this matters:");
        whyIcon->setStyleSheet(
            QString("color: %1; font-size: 12px; font-weight: bold;").arg(C::yellow()));
        whyIcon->setFixedWidth(110);
        whyRow->addWidget(whyIcon);

        auto* whyLabel = new QLabel(f.whyItMatters);
        whyLabel->setWordWrap(true);
        whyLabel->setStyleSheet(QString("color: %1; font-size: 12px; font-style: italic;").arg(C::fg()));
        whyRow->addWidget(whyLabel, 1);

        cardLayout->addLayout(whyRow);
    }

    // ── Fix suggestion ────────────────────────────────────────────────────────
    if (!f.fixSuggestion.isEmpty()) {
        auto* fixRow = new QHBoxLayout;
        fixRow->setSpacing(6);

        auto* fixIcon = new QLabel("Fix:");
        fixIcon->setStyleSheet(QString(
            "color: %1; font-size: 12px; font-weight: bold;").arg(color));
        fixIcon->setFixedWidth(28);
        fixRow->addWidget(fixIcon);

        auto* fixLabel = new QLabel(f.fixSuggestion);
        fixLabel->setWordWrap(true);
        fixLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(C::fg2()));
        fixRow->addWidget(fixLabel, 1);

        cardLayout->addLayout(fixRow);
    }

    // ── "Try It Yourself" button ──────────────────────────────────────────────
    if (!f.vulnType.isEmpty()) {
        auto* tryRow = new QHBoxLayout;
        tryRow->addStretch(1);

        auto* tryBtn = new QPushButton("Try It Yourself \u2192");
        tryBtn->setFixedHeight(24);
        tryBtn->setCursor(Qt::PointingHandCursor);
        tryBtn->setStyleSheet(
            QString("QPushButton { background: none; color: %1; border: 1px solid %2;"
            " border-radius: 4px; padding: 0 10px; font-size: 12px; }"
            "QPushButton:hover { background: %3; color: %1; }")
            .arg(C::accent(), C::border(), C::bg2()));
        QString vt = f.vulnType;
        connect(tryBtn, &QPushButton::clicked, this, [this, vt]() {
            emit openLab(vt);
        });
        tryRow->addWidget(tryBtn);
        cardLayout->addLayout(tryRow);
    }

    return card;
}

// ── Severity helpers ─────────────────────────────────────────────────────────
QString SecurityTestingFrame::severityColor(SecurityFinding::Severity sev)
{
    switch (sev) {
    case SecurityFinding::CRITICAL: return C::red();
    case SecurityFinding::HIGH:     return "#fab387";  // Peach — not in Colors struct
    case SecurityFinding::MEDIUM:   return C::yellow();
    case SecurityFinding::LOW:      return C::accent();
    }
    return C::accent();
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

// ── CVE Auto-Check wiring ─────────────────────────────────────────────────────

void SecurityTestingFrame::setCVEMonitor(CVEMonitor* monitor)
{
    m_cveMonitor = monitor;
    if (m_cveMonitor) {
        connect(m_cveMonitor, &CVEMonitor::auditCompleted,
                this, &SecurityTestingFrame::onCVEAuditCompleted,
                Qt::UniqueConnection);
    }
}

void SecurityTestingFrame::reloadCVESettings()
{
    if (m_cveEnabledChk)
        m_cveEnabledChk->setChecked(CVEMonitor::loadEnabled());
    if (m_cveFreqCombo) {
        int ms = CVEMonitor::loadFrequencyMs();
        for (int i = 0; i < m_cveFreqCombo->count(); ++i) {
            if (m_cveFreqCombo->itemData(i).toInt() == ms) {
                m_cveFreqCombo->setCurrentIndex(i);
                break;
            }
        }
    }
}

void SecurityTestingFrame::onCVEToggled(bool enabled)
{
    CVEMonitor::saveEnabled(enabled);
    if (m_cveMonitor) {
        m_cveMonitor->setEnabled(enabled);
        if (enabled) {
            m_cveStatusLabel->setText("Monitoring enabled. Next check will run on schedule.");
        } else {
            m_cveStatusLabel->setText("Monitoring disabled.");
        }
    }
    emit cveSettingsChanged();
}

void SecurityTestingFrame::onCVEFrequencyChanged(int index)
{
    if (!m_cveFreqCombo) return;
    int ms = m_cveFreqCombo->itemData(index).toInt();
    CVEMonitor::saveFrequencyMs(ms);
    if (m_cveMonitor)
        m_cveMonitor->setFrequencyMs(ms);
    emit cveSettingsChanged();
}

void SecurityTestingFrame::onRunCVENow()
{
    if (!m_cveMonitor) {
        m_cveStatusLabel->setText("CVE monitor not available.");
        return;
    }
    m_cveStatusLabel->setText("Running pip-audit...");
    m_cveRunNowBtn->setEnabled(false);
    m_cveMonitor->runNow();
}

void SecurityTestingFrame::onCVEAuditCompleted(int count, const QStringList& packages)
{
    m_cveRunNowBtn->setEnabled(true);
    if (count == 0) {
        m_cveStatusLabel->setText("Last check: No vulnerabilities found.");
        m_cveStatusLabel->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(C::green()));
    } else {
        m_cveStatusLabel->setText(
            QString("Last check: %1 CVE(s) found in: %2")
                .arg(count)
                .arg(packages.join(", ")));
        m_cveStatusLabel->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(C::red()));
    }
}
