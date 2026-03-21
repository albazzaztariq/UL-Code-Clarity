#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>

// ============================================================================
// SideBySideDiffWidget — shows original code on left and suggested on right
//
// - Synchronized vertical scrolling between both panes
// - Changed lines highlighted: light red on left, light green on right
// - "Apply" button accepts the suggestion (emits applied())
// - "Skip"  button dismisses the entry  (emits skipped())
// ============================================================================
class SideBySideDiffWidget : public QWidget {
    Q_OBJECT

public:
    explicit SideBySideDiffWidget(QWidget* parent = nullptr);

    // Set the code to display. originalCode and suggestedCode are full snippets.
    void setContent(const QString& title,
                    const QString& originalCode,
                    const QString& suggestedCode);

signals:
    void applied();   // user clicked Apply
    void skipped();   // user clicked Skip

private slots:
    void syncScrollLeft(int value);
    void syncScrollRight(int value);

private:
    void highlightDiffs();

    QLabel*         m_titleLabel   = nullptr;
    QPlainTextEdit* m_leftPane     = nullptr;   // original
    QPlainTextEdit* m_rightPane    = nullptr;   // suggested
    QPushButton*    m_applyBtn     = nullptr;
    QPushButton*    m_skipBtn      = nullptr;

    bool m_syncing = false; // prevent scroll feedback loop
};
