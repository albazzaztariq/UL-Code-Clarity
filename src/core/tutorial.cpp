#include "core/tutorial.h"
#include "core/jsonloader.h"

#include <QScrollArea>
#include <QScrollBar>
#include <QJsonObject>

// ============================================================================
// Load pages from tutorials.json
// ============================================================================

static QVector<TutorialDialog::Page> loadPages(const QString& key)
{
    QJsonArray arr = JsonLoader::loadArray("tutorials.json", key);
    QVector<TutorialDialog::Page> pages;
    pages.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        QJsonObject obj = v.toObject();
        TutorialDialog::Page p;
        p.title = obj["title"].toString();
        p.html  = obj["html"].toString();
        pages.append(p);
    }
    return pages;
}

// ============================================================================
// Factory methods
// ============================================================================

TutorialDialog* TutorialDialog::buildTarget(QWidget* parent)
{
    return new TutorialDialog("Build Target", loadPages("build_target"), parent);
}

TutorialDialog* TutorialDialog::memory(QWidget* parent)
{
    return new TutorialDialog("Memory Management", loadPages("memory"), parent);
}

TutorialDialog* TutorialDialog::security(QWidget* parent)
{
    return new TutorialDialog("Security", loadPages("security"), parent);
}

TutorialDialog* TutorialDialog::codeHealth(QWidget* parent)
{
    return new TutorialDialog("Code Health", loadPages("code_health"), parent);
}

// ============================================================================
// Constructor — builds the dialog UI
// ============================================================================

TutorialDialog::TutorialDialog(const QString& tutorialTitle,
                               const QVector<Page>& pages,
                               QWidget* parent)
    : QDialog(parent), m_pages(pages)
{
    setWindowTitle(tutorialTitle);
    setMinimumSize(560, 440);
    setStyleSheet("background: #1e1e2e; color: #cdd6f4;");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 22, 28, 18);
    root->setSpacing(14);

    // Title
    m_titleLabel = new QLabel;
    m_titleLabel->setStyleSheet(
        "color: #89b4fa; font-size: 16px; font-weight: 700;");
    m_titleLabel->setWordWrap(true);
    root->addWidget(m_titleLabel);

    // Scroll area + body
    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 7px; background: transparent; }"
        "QScrollBar::handle:vertical { background: #3c3c54; border-radius: 4px; }");

    m_bodyLabel = new QLabel;
    m_bodyLabel->setWordWrap(true);
    m_bodyLabel->setTextFormat(Qt::RichText);
    m_bodyLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_bodyLabel->setStyleSheet("color: #cdd6f4; font-size: 13px; background: transparent;");
    m_bodyLabel->setContentsMargins(0, 0, 12, 0);

    m_scrollArea->setWidget(m_bodyLabel);
    root->addWidget(m_scrollArea, 1);

    // Dots
    m_dotsLayout = new QHBoxLayout;
    m_dotsLayout->setSpacing(6);
    m_dotsLayout->addStretch();
    for (int i = 0; i < m_pages.size(); ++i) {
        auto* dot = new QLabel("\xe2\x97\x8f");
        dot->setStyleSheet("font-size: 8px;");
        m_dotsLayout->addWidget(dot);
        m_dots.append(dot);
    }
    m_dotsLayout->addStretch();
    root->addLayout(m_dotsLayout);

    // Navigation buttons
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    m_backBtn = new QPushButton("Back");
    m_backBtn->setFixedSize(90, 30);
    m_backBtn->setCursor(Qt::PointingHandCursor);
    m_backBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; font-size: 13px; }"
        "QPushButton:hover { background: #45475a; }");
    connect(m_backBtn, &QPushButton::clicked, this, &TutorialDialog::goBack);

    m_nextBtn = new QPushButton("Next");
    m_nextBtn->setFixedSize(90, 30);
    m_nextBtn->setCursor(Qt::PointingHandCursor);
    m_nextBtn->setStyleSheet(
        "QPushButton { background: #89b4fa; color: #1e1e2e; border: none;"
        " border-radius: 4px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: #b4d0fb; }");
    connect(m_nextBtn, &QPushButton::clicked, this, &TutorialDialog::goNext);

    auto* closeBtn = new QPushButton("Close");
    closeBtn->setFixedSize(80, 30);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; font-size: 13px; }"
        "QPushButton:hover { background: #45475a; }");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    btnRow->addWidget(m_backBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_nextBtn);
    btnRow->addWidget(closeBtn);
    root->addLayout(btnRow);

    showPage(0);
}

void TutorialDialog::showPage(int index)
{
    if (index < 0 || index >= m_pages.size()) return;
    m_current = index;

    const Page& p = m_pages[index];
    m_titleLabel->setText(p.title);
    m_bodyLabel->setText(p.html);
    m_scrollArea->verticalScrollBar()->setValue(0);

    m_backBtn->setEnabled(index > 0);
    m_nextBtn->setText(index == m_pages.size() - 1 ? "Finish" : "Next");

    updateDots();
}

void TutorialDialog::updateDots()
{
    for (int i = 0; i < m_dots.size(); ++i) {
        m_dots[i]->setStyleSheet(
            i == m_current
                ? "font-size: 8px; color: #89b4fa;"
                : "font-size: 8px; color: #45475a;");
    }
}

void TutorialDialog::goNext()
{
    if (m_current < m_pages.size() - 1)
        showPage(m_current + 1);
    else
        accept();
}

void TutorialDialog::goBack()
{
    if (m_current > 0)
        showPage(m_current - 1);
}
