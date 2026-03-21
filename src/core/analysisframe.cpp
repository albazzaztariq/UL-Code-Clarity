#include "core/analysisframe.h"
#include <QFrame>

AnalysisFrame::AnalysisFrame(const QString& title, QWidget* parent)
    : QWidget(parent)
{
    buildUi(title);
}

void AnalysisFrame::buildUi(const QString& title)
{
    setStyleSheet("background: #1e1e2e; color: #cdd6f4;");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12);
    root->setSpacing(10);

    // ── Header bar ──────────────────────────────────────────────────────────
    auto* topBar = new QHBoxLayout;
    topBar->setSpacing(8);

    auto* backBtn = new QPushButton("Back to Editor");
    backBtn->setFixedHeight(30);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; padding: 0 14px; font-size: 13px; }"
        "QPushButton:hover { background: #45475a; }");
    connect(backBtn, &QPushButton::clicked, this, &AnalysisFrame::backToEditor);
    topBar->addWidget(backBtn);

    topBar->addStretch(1);

    auto* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet("color: #cdd6f4; font-size: 18px; font-weight: bold;");
    topBar->addWidget(titleLabel);

    topBar->addStretch(1);

    auto* helpBtn = new QPushButton("?");
    helpBtn->setFixedSize(36, 36);
    helpBtn->setCursor(Qt::PointingHandCursor);
    helpBtn->setStyleSheet(
        "QPushButton { background: #89b4fa; color: #1e1e2e; border: none;"
        " border-radius: 18px; font-size: 18px; font-weight: bold; }"
        "QPushButton:hover { background: #b4d0fb; }");
    connect(helpBtn, &QPushButton::clicked, this, &AnalysisFrame::onHelpClicked);
    topBar->addWidget(helpBtn);

    root->addLayout(topBar);

    // Separator
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #313244;");
    root->addWidget(sep);

    // Optional subclass control bar
    QWidget* controls = buildControls();
    if (controls)
        root->addWidget(controls);

    // ── Results scroll area ──────────────────────────────────────────────────
    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(
        "QScrollArea { border: 1px solid #313244; background: #181825; border-radius: 4px; }"
        "QScrollBar:vertical { background: #1e1e2e; width: 8px; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background: #45475a; border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    m_resultsWidget = new QWidget;
    m_resultsWidget->setStyleSheet("background: #181825;");
    m_resultsLayout = new QVBoxLayout(m_resultsWidget);
    m_resultsLayout->setContentsMargins(8, 8, 8, 8);
    m_resultsLayout->setSpacing(6);
    m_resultsLayout->addStretch(1);

    m_scrollArea->setWidget(m_resultsWidget);
    root->addWidget(m_scrollArea, 1);

    // ── Status bar ───────────────────────────────────────────────────────────
    m_statusLabel = new QLabel;
    m_statusLabel->setStyleSheet("color: #6c7086; font-size: 11px;");
    root->addWidget(m_statusLabel);
}

void AnalysisFrame::setCode(const QString& code, const QString& language)
{
    m_code     = code;
    m_language = language;
}

void AnalysisFrame::onHelpClicked()
{
    showHelp();
}

void AnalysisFrame::clearResults()
{
    QLayoutItem* item;
    while ((item = m_resultsLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    m_resultsLayout->addStretch(1);
}

void AnalysisFrame::addResultWidget(QWidget* w)
{
    // Insert before the trailing stretch
    int count = m_resultsLayout->count();
    m_resultsLayout->insertWidget(count - 1, w);
}

void AnalysisFrame::setStatus(const QString& text)
{
    m_statusLabel->setText(text);
}
