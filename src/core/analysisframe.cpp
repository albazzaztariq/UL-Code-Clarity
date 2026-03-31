#include "core/analysisframe.h"
#include <QDebug>
#include <QFrame>

AnalysisFrame::AnalysisFrame(const QString& title, QWidget* parent)
    : QWidget(parent)
{
    buildUi(title);
}

void AnalysisFrame::buildUi(const QString& title)
{
    qDebug() << "[DEBUG] AnalysisFrame::buildUi";
    setStyleSheet("background: #1e1e2e; color: #cdd6f4;");

    const QString backBtnStyle =
        "QPushButton { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; padding: 0 14px; font-size: 13px; }"
        "QPushButton:hover { background: #45475a; }";
    const QString titleStyle = "color: #cdd6f4; font-size: 18px; font-weight: bold;";
    const QString helpStyle =
        "QPushButton { background: #89b4fa; color: #1e1e2e; border: none;"
        " border-radius: 18px; font-size: 18px; font-weight: bold; }"
        "QPushButton:hover { background: #b4d0fb; }";
    const QString separatorStyle = "color: #313244;";
    const QString scrollStyle =
        "QScrollArea { border: 1px solid #313244; background: #181825; border-radius: 4px; }"
        "QScrollBar:vertical { background: #1e1e2e; width: 8px; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background: #45475a; border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }";
    const QString resultsStyle = "background: #181825;";
    const QString statusStyle = "color: #6c7086; font-size: 11px;";

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12);
    root->setSpacing(10);

    // ── Header bar ──────────────────────────────────────────────────────────
    auto* topBar = new QHBoxLayout;
    topBar->setSpacing(8);

    auto* backBtn = new QPushButton("Back to Editor");
    backBtn->setFixedHeight(30);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(backBtnStyle);
    connect(backBtn, &QPushButton::clicked, this, &AnalysisFrame::backToEditor);
    topBar->addWidget(backBtn);

    topBar->addStretch(1);

    auto* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet(titleStyle);
    topBar->addWidget(titleLabel);

    topBar->addStretch(1);

    auto* helpBtn = new QPushButton("?");
    helpBtn->setFixedSize(36, 36);
    helpBtn->setCursor(Qt::PointingHandCursor);
    helpBtn->setStyleSheet(helpStyle);
    connect(helpBtn, &QPushButton::clicked, this, &AnalysisFrame::onHelpClicked);
    topBar->addWidget(helpBtn);

    root->addLayout(topBar);

    // Separator
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(separatorStyle);
    root->addWidget(sep);

    // Optional subclass control bar
    QWidget* controls = buildControls();
    if (controls)
        root->addWidget(controls);

    // ── Results scroll area ──────────────────────────────────────────────────
    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(scrollStyle);

    m_resultsWidget = new QWidget;
    m_resultsWidget->setStyleSheet(resultsStyle);
    m_resultsLayout = new QVBoxLayout(m_resultsWidget);
    m_resultsLayout->setContentsMargins(8, 8, 8, 8);
    m_resultsLayout->setSpacing(6);
    m_resultsLayout->addStretch(1);

    m_scrollArea->setWidget(m_resultsWidget);
    root->addWidget(m_scrollArea, 1);

    // ── Status bar ───────────────────────────────────────────────────────────
    m_statusLabel = new QLabel;
    m_statusLabel->setStyleSheet(statusStyle);
    root->addWidget(m_statusLabel);
}

void AnalysisFrame::setCode(const QString& code, const QString& language)
{
    qDebug() << "[DEBUG] AnalysisFrame::setCode" << language;
    m_code     = code;
    m_language = language;
}

void AnalysisFrame::onHelpClicked()
{
    qDebug() << "[DEBUG] AnalysisFrame::onHelpClicked";
    showHelp();
}

void AnalysisFrame::clearResults()
{
    qDebug() << "[DEBUG] AnalysisFrame::clearResults start";
    int removed = 0;
    QLayoutItem* item;
    while ((item = m_resultsLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
        ++removed;
    }
    m_resultsLayout->addStretch(1);
    qDebug() << "[DEBUG] AnalysisFrame::clearResults removed" << removed;
}

void AnalysisFrame::addResultWidget(QWidget* w)
{
    qDebug() << "[DEBUG] AnalysisFrame::addResultWidget";
    // Insert before the trailing stretch
    int count = m_resultsLayout->count();
    m_resultsLayout->insertWidget(count - 1, w);
}

void AnalysisFrame::setStatus(const QString& text)
{
    qDebug() << "[DEBUG] AnalysisFrame::setStatus";
    m_statusLabel->setText(text);
}
