#include "core/buildbar.h"
#include "core/theme.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>

BuildBar::BuildBar(QWidget* parent)
    : QWidget(parent)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // ── Results Pane (above the bar, hidden by default) ──────────────
    m_resultsPane = new QWidget;
    m_resultsPane->setStyleSheet(
        "background: #181825; border-top: 1px solid #313244;");
    m_resultsPane->setMaximumHeight(140);
    m_resultsPane->hide();

    auto* resultsLayout = new QVBoxLayout(m_resultsPane);
    resultsLayout->setContentsMargins(12, 6, 12, 6);
    resultsLayout->setSpacing(4);

    // Results header
    auto* resultsHeader = new QWidget;
    auto* headerLayout = new QHBoxLayout(resultsHeader);
    headerLayout->setContentsMargins(0, 0, 0, 0);

    m_resultsTitle = new QLabel("Output");
    m_resultsTitle->setStyleSheet(
        "QLabel { color: #a6adc8; font-size: 10px; }");
    headerLayout->addWidget(m_resultsTitle);
    headerLayout->addStretch();

    m_resultsClose = new QPushButton(QChar(0x2715));  // ✕
    m_resultsClose->setFixedSize(16, 16);
    m_resultsClose->setStyleSheet(
        "QPushButton { color: #6c7086; background: transparent;"
        " border: none; font-size: 11px; padding: 0; }"
        "QPushButton:hover { color: #cdd6f4; }");
    headerLayout->addWidget(m_resultsClose);

    resultsLayout->addWidget(resultsHeader);

    m_resultsContent = new QTextEdit;
    m_resultsContent->setReadOnly(true);
    m_resultsContent->setStyleSheet(
        "QTextEdit { background: #181825; color: #a6e3a1;"
        " border: none; font-family: 'Cascadia Code', 'Consolas', monospace;"
        " font-size: 11px; }");
    resultsLayout->addWidget(m_resultsContent);

    outerLayout->addWidget(m_resultsPane);

    // ── Build Bar ───────────────────────────────────────────────────
    m_bar = new QWidget;
    auto* bar = m_bar;
    bar->setFixedHeight(38);
    bar->setStyleSheet("background: #2a2a3c; border-top: 1px solid #313244;");

    auto* barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(12, 0, 12, 0);
    barLayout->setSpacing(6);

    // Run button (green, bold, proper sizing)
    m_runBtn = new QPushButton(QString::fromUtf8("\xe2\x96\xb6") + "  Run");
    m_runBtn->setFixedHeight(26);
    m_runBtn->setStyleSheet(
        "QPushButton { background: #a6e3a1; color: #1e1e2e;"
        " font-weight: 700; font-size: 12px; padding: 0 16px; border-radius: 6px; }"
        "QPushButton:hover { background: #b9f0b4; }"
        "QPushButton:pressed { background: #94d08f; }");
    barLayout->addWidget(m_runBtn);

    // Build button
    m_buildBtn = new QPushButton("Build");
    m_buildBtn->setFixedHeight(26);
    m_buildBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #cdd6f4;"
        " font-size: 12px; padding: 0 14px; border-radius: 6px;"
        " border: 1px solid #45475a; }"
        "QPushButton:hover { background: #3c3c54; }"
        "QPushButton:pressed { background: #45475a; }");
    barLayout->addWidget(m_buildBtn);

    // MemCheck button (only for C, hidden by default)
    m_memCheckBtn = new QPushButton("Mem Check");
    m_memCheckBtn->setFixedHeight(26);
    m_memCheckBtn->setStyleSheet(
        "QPushButton { background: rgba(250,179,135,0.15); color: #fab387;"
        " font-weight: 600; font-size: 11px; padding: 0 12px; border-radius: 6px;"
        " border: 1px solid rgba(250,179,135,0.3); }"
        "QPushButton:hover { background: rgba(250,179,135,0.25); }");
    m_memCheckBtn->hide();
    barLayout->addWidget(m_memCheckBtn);

    // Separator
    m_barSep = new QFrame;
    m_barSep->setFrameShape(QFrame::VLine);
    m_barSep->setFixedHeight(16);
    m_barSep->setStyleSheet("color: #313244;");
    barLayout->addWidget(m_barSep);

    // Target label + help button + combo
    m_targetLabel = new QLabel("Build Target:");
    m_targetLabel->setStyleSheet("QLabel { color: #a6adc8; font-size: 11px; }");
    barLayout->addWidget(m_targetLabel);

    m_targetHelpBtn = new QPushButton("?");
    auto* targetHelpBtn = m_targetHelpBtn;
    targetHelpBtn->setFixedSize(24, 24);
    targetHelpBtn->setCursor(Qt::PointingHandCursor);
    targetHelpBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #a6adc8;"
        " border-radius: 12px; font-size: 14px; padding: 0; border: none; }"
        "QPushButton:hover { color: #cdd6f4; }");
    targetHelpBtn->setToolTip(
        "Select your compilation target. Native targets produce standalone binaries. "
        "Language targets produce source code in that language.");
    barLayout->addWidget(targetHelpBtn);

    m_targetCombo = new QComboBox;
    m_targetCombo->setFixedHeight(26);
    // Native targets
    m_targetCombo->addItem("Windows x64");
    m_targetCombo->addItem("Windows ARM64");
    m_targetCombo->addItem("macOS x64 (Intel)");
    m_targetCombo->addItem("macOS ARM64 (Apple Silicon)");
    m_targetCombo->addItem("Linux x64");
    m_targetCombo->addItem("Linux ARM64");
    m_targetCombo->addItem("All Windows (installer)");
    m_targetCombo->addItem("All macOS (universal binary)");
    // Language targets
    m_targetCombo->addItem("Python (.py output)");
    m_targetCombo->addItem("JavaScript (.js output)");
    m_targetCombo->addItem("WASM (browser)");
    m_targetCombo->addItem("Bytecode VM");
    m_targetCombo->setCurrentIndex(0);  // Default: Windows x64
    barLayout->addWidget(m_targetCombo);

    barLayout->addStretch();

    // Output toggle button
    m_outputToggle = new QPushButton("Output");
    m_outputToggle->setFixedHeight(26);
    m_outputToggle->setStyleSheet(
        "QPushButton { background: #313244; color: #a6adc8;"
        " font-size: 11px; padding: 0 12px; border-radius: 6px;"
        " border: 1px solid #45475a; }"
        "QPushButton:hover { background: #3c3c54; }");
    barLayout->addWidget(m_outputToggle);

    outerLayout->addWidget(bar);

    // Connections
    connect(m_runBtn, &QPushButton::clicked, this, &BuildBar::runRequested);
    connect(m_buildBtn, &QPushButton::clicked, this, &BuildBar::buildRequested);
    connect(m_memCheckBtn, &QPushButton::clicked, this, &BuildBar::memCheckRequested);
    connect(m_outputToggle, &QPushButton::clicked, this, &BuildBar::toggleResults);
    connect(m_resultsClose, &QPushButton::clicked, this, [this]() {
        m_resultsPane->hide();
        m_resultsVisible = false;
        if (m_isDark) {
            m_outputToggle->setStyleSheet(
                "QPushButton { background: #313244; color: #a6adc8;"
                " font-size: 11px; padding: 0 12px; border-radius: 6px;"
                " border: 1px solid #45475a; }"
                "QPushButton:hover { background: #3c3c54; }");
        } else {
            m_outputToggle->setStyleSheet(
                "QPushButton { background: #e8e8e8; color: #555555;"
                " font-size: 11px; padding: 0 12px; border-radius: 6px;"
                " border: 1px solid #d0d0d0; }"
                "QPushButton:hover { background: #d8d8d8; }");
        }
    });
}

void BuildBar::showOutput(const QString& text)
{
    m_resultsTitle->setText("Output");
    if (m_isDark) {
        m_resultsContent->setStyleSheet(
            "QTextEdit { background: #181825; color: #a6e3a1;"
            " border: none; font-family: 'Cascadia Code', 'Consolas', monospace;"
            " font-size: 11px; }");
        m_outputToggle->setStyleSheet(
            "QPushButton { background: #333348; color: #a6e3a1;"
            " font-size: 11px; padding: 4px 12px; border-radius: 6px; }"
            "QPushButton:hover { background: #3c3c54; }");
    } else {
        m_resultsContent->setStyleSheet(
            "QTextEdit { background: #f8f8f8; color: #1a7a1a;"
            " border: none; font-family: 'Cascadia Code', 'Consolas', monospace;"
            " font-size: 11px; }");
        m_outputToggle->setStyleSheet(
            "QPushButton { background: #e8e8e8; color: #1a7a1a;"
            " font-size: 11px; padding: 4px 12px; border-radius: 6px;"
            " border: 1px solid #d0d0d0; }"
            "QPushButton:hover { background: #d8d8d8; }");
    }
    m_resultsContent->setPlainText(text);
    m_resultsPane->show();
    m_resultsVisible = true;
}

void BuildBar::showError(const QString& text)
{
    m_resultsTitle->setText("Error");
    if (m_isDark) {
        m_resultsContent->setStyleSheet(
            "QTextEdit { background: #181825; color: #f38ba8;"
            " border: none; font-family: 'Cascadia Code', 'Consolas', monospace;"
            " font-size: 11px; }");
    } else {
        m_resultsContent->setStyleSheet(
            "QTextEdit { background: #fff0f0; color: #cc0000;"
            " border: none; font-family: 'Cascadia Code', 'Consolas', monospace;"
            " font-size: 11px; }");
    }
    m_resultsContent->setPlainText(text);
    m_resultsPane->show();
    m_resultsVisible = true;
}

void BuildBar::showMemCheckResults(const QString& html)
{
    m_resultsTitle->setText("Memory Check");
    m_resultsContent->setHtml(html);
    m_resultsPane->show();
    m_resultsVisible = true;
}

void BuildBar::setMemCheckVisible(bool visible)
{
    m_memCheckBtn->setVisible(visible);
}

void BuildBar::toggleResults()
{
    m_resultsVisible = !m_resultsVisible;
    m_resultsPane->setVisible(m_resultsVisible);
}

void BuildBar::clearResults()
{
    m_resultsContent->clear();
    m_resultsPane->hide();
    m_resultsVisible = false;
}

void BuildBar::showResults()
{
    m_resultsPane->show();
    m_resultsVisible = true;
}

void BuildBar::applyTheme(bool isDark)
{
    m_isDark = isDark;

    if (isDark) {
        m_resultsPane->setStyleSheet(
            "background: #181825; border-top: 1px solid #313244;");
        m_resultsTitle->setStyleSheet(
            "QLabel { color: #a6adc8; font-size: 10px; }");
        m_resultsClose->setStyleSheet(
            "QPushButton { color: #6c7086; background: transparent;"
            " border: none; font-size: 11px; padding: 0; }"
            "QPushButton:hover { color: #cdd6f4; }");
        m_resultsContent->setStyleSheet(
            "QTextEdit { background: #181825; color: #a6e3a1;"
            " border: none; font-family: 'Cascadia Code', 'Consolas', monospace;"
            " font-size: 11px; }");
        m_bar->setStyleSheet("background: #2a2a3c; border-top: 1px solid #313244;");
        m_buildBtn->setStyleSheet(
            "QPushButton { background: #313244; color: #cdd6f4;"
            " font-size: 12px; padding: 0 14px; border-radius: 6px;"
            " border: 1px solid #45475a; }"
            "QPushButton:hover { background: #3c3c54; }"
            "QPushButton:pressed { background: #45475a; }");
        m_outputToggle->setStyleSheet(
            "QPushButton { background: #313244; color: #a6adc8;"
            " font-size: 11px; padding: 0 12px; border-radius: 6px;"
            " border: 1px solid #45475a; }"
            "QPushButton:hover { background: #3c3c54; }");
        m_barSep->setStyleSheet("color: #313244;");
        m_targetLabel->setStyleSheet("QLabel { color: #a6adc8; font-size: 11px; }");
        m_targetHelpBtn->setStyleSheet(
            "QPushButton { background: transparent; color: #a6adc8;"
            " border-radius: 12px; font-size: 14px; padding: 0; border: none; }"
            "QPushButton:hover { color: #cdd6f4; }");
    } else {
        m_resultsPane->setStyleSheet(
            "background: #f0f0f0; border-top: 1px solid #d0d0d0;");
        m_resultsTitle->setStyleSheet(
            "QLabel { color: #555555; font-size: 10px; }");
        m_resultsClose->setStyleSheet(
            "QPushButton { color: #999999; background: transparent;"
            " border: none; font-size: 11px; padding: 0; }"
            "QPushButton:hover { color: #1e1e2e; }");
        m_resultsContent->setStyleSheet(
            "QTextEdit { background: #f8f8f8; color: #1a7a1a;"
            " border: none; font-family: 'Cascadia Code', 'Consolas', monospace;"
            " font-size: 11px; }");
        m_bar->setStyleSheet("background: #f0f0f0; border-top: 1px solid #d0d0d0;");
        m_buildBtn->setStyleSheet(
            "QPushButton { background: #e8e8e8; color: #1e1e2e;"
            " font-size: 12px; padding: 0 14px; border-radius: 6px;"
            " border: 1px solid #d0d0d0; }"
            "QPushButton:hover { background: #d8d8d8; }"
            "QPushButton:pressed { background: #c8c8c8; }");
        m_outputToggle->setStyleSheet(
            "QPushButton { background: #e8e8e8; color: #555555;"
            " font-size: 11px; padding: 0 12px; border-radius: 6px;"
            " border: 1px solid #d0d0d0; }"
            "QPushButton:hover { background: #d8d8d8; }");
        m_barSep->setStyleSheet("color: #d0d0d0;");
        m_targetLabel->setStyleSheet("QLabel { color: #555555; font-size: 11px; }");
        m_targetHelpBtn->setStyleSheet(
            "QPushButton { background: transparent; color: #555555;"
            " border-radius: 12px; font-size: 14px; padding: 0; border: none; }"
            "QPushButton:hover { color: #1e1e2e; }");
    }
}
