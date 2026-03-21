#include "core/diffview.h"
#include "core/theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>

SideBySideDiffWidget::SideBySideDiffWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(700, 400);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    // Title
    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet(
        "QLabel { color: #cdd6f4; font-size: 14px; font-weight: bold; "
        "background: transparent; }");
    root->addWidget(m_titleLabel);

    // Column headers
    auto* headerRow = new QHBoxLayout;
    auto* leftHeader  = new QLabel("Original", this);
    auto* rightHeader = new QLabel("Suggested", this);
    leftHeader->setStyleSheet(
        "QLabel { color: #f38ba8; font-size: 11px; font-weight: bold; "
        "background: transparent; padding: 2px 4px; }");
    rightHeader->setStyleSheet(
        "QLabel { color: #a6e3a1; font-size: 11px; font-weight: bold; "
        "background: transparent; padding: 2px 4px; }");
    headerRow->addWidget(leftHeader);
    headerRow->addWidget(rightHeader);
    root->addLayout(headerRow);

    // Side-by-side panes
    auto* panesRow = new QHBoxLayout;
    panesRow->setSpacing(4);

    m_leftPane = new QPlainTextEdit(this);
    m_leftPane->setReadOnly(true);
    m_leftPane->setFont(QFont(Theme::MonoFont, 11));
    m_leftPane->setStyleSheet(
        "QPlainTextEdit { background: #2a1a1e; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; }");

    m_rightPane = new QPlainTextEdit(this);
    m_rightPane->setReadOnly(true);
    m_rightPane->setFont(QFont(Theme::MonoFont, 11));
    m_rightPane->setStyleSheet(
        "QPlainTextEdit { background: #1a2a1e; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; }");

    panesRow->addWidget(m_leftPane);
    panesRow->addWidget(m_rightPane);
    root->addLayout(panesRow, 1);

    // Buttons
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();

    m_skipBtn = new QPushButton("Skip", this);
    m_skipBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #a6adc8; border-radius: 6px;"
        " padding: 6px 20px; font-size: 12px; }"
        "QPushButton:hover { background: #45475a; }");

    m_applyBtn = new QPushButton("Apply", this);
    m_applyBtn->setStyleSheet(
        "QPushButton { background: #a6e3a1; color: #1e1e2e; border-radius: 6px;"
        " padding: 6px 20px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background: #89d98e; }");

    btnRow->addWidget(m_skipBtn);
    btnRow->addWidget(m_applyBtn);
    root->addLayout(btnRow);

    // Synchronized scrolling
    connect(m_leftPane->verticalScrollBar(),  &QScrollBar::valueChanged,
            this, &SideBySideDiffWidget::syncScrollLeft);
    connect(m_rightPane->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &SideBySideDiffWidget::syncScrollRight);

    connect(m_applyBtn, &QPushButton::clicked, this, &SideBySideDiffWidget::applied);
    connect(m_skipBtn,  &QPushButton::clicked, this, &SideBySideDiffWidget::skipped);
}

void SideBySideDiffWidget::setContent(const QString& title,
                                       const QString& originalCode,
                                       const QString& suggestedCode)
{
    m_titleLabel->setText(title);
    m_leftPane->setPlainText(originalCode);
    m_rightPane->setPlainText(suggestedCode);
    highlightDiffs();
}

// ── Synchronized scrolling ────────────────────────────────────────────────
void SideBySideDiffWidget::syncScrollLeft(int value)
{
    if (m_syncing) return;
    m_syncing = true;
    m_rightPane->verticalScrollBar()->setValue(value);
    m_syncing = false;
}

void SideBySideDiffWidget::syncScrollRight(int value)
{
    if (m_syncing) return;
    m_syncing = true;
    m_leftPane->verticalScrollBar()->setValue(value);
    m_syncing = false;
}

// ── Diff highlighting ─────────────────────────────────────────────────────
// Lines that differ between left and right get a tinted background.
void SideBySideDiffWidget::highlightDiffs()
{
    QStringList leftLines  = m_leftPane->toPlainText().split('\n');
    QStringList rightLines = m_rightPane->toPlainText().split('\n');

    int maxLines = qMax(leftLines.size(), rightLines.size());

    QList<QTextEdit::ExtraSelection> leftSels, rightSels;

    QColor leftHighlight  = QColor(0xf3, 0x8b, 0xa8, 60);  // red tint
    QColor rightHighlight = QColor(0xa6, 0xe3, 0xa1, 60);   // green tint

    auto makeSelection = [](QPlainTextEdit* pane, int lineIdx, const QColor& bg)
        -> QTextEdit::ExtraSelection
    {
        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(bg);
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);

        QTextCursor cursor(pane->document()->findBlockByLineNumber(lineIdx));
        sel.cursor = cursor;
        return sel;
    };

    for (int i = 0; i < maxLines; ++i) {
        QString l = (i < leftLines.size())  ? leftLines[i]  : QString();
        QString r = (i < rightLines.size()) ? rightLines[i] : QString();
        if (l != r) {
            if (i < leftLines.size())
                leftSels.append(makeSelection(m_leftPane, i, leftHighlight));
            if (i < rightLines.size())
                rightSels.append(makeSelection(m_rightPane, i, rightHighlight));
        }
    }

    m_leftPane->setExtraSelections(leftSels);
    m_rightPane->setExtraSelections(rightSels);
}
