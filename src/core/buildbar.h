#pragma once

#include <QWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QFrame>

class BuildBar : public QWidget {
    Q_OBJECT

public:
    explicit BuildBar(QWidget* parent = nullptr);

    void showOutput(const QString& text);
    void showError(const QString& text);
    void showMemCheckResults(const QString& html);
    void setMemCheckVisible(bool visible);
    void toggleResults();
    void clearResults();
    void showResults();   // show results pane (without replacing content)
    void applyTheme(bool isDark);
    void updatePredictVisibility();

    // Direct access to the output text edit for streaming output
    QTextEdit* resultsContent() const { return m_resultsContent; }

    // Build chain selection
    QString currentChainName() const;

    // Update the error badge text and visibility
    void setErrorBadge(int count);

signals:
    void runRequested();
    void buildRequested();
    void memCheckRequested();
    void debugRequested();
    void predictToggled(bool on);
    void errorJournalRequested();
    void customizeLayoutRequested();

private:
    QPushButton* m_runBtn;
    QPushButton* m_buildBtn;
    QPushButton* m_debugBtn;
    QPushButton* m_memCheckBtn;
    QPushButton* m_predictToggle = nullptr;
    QPushButton* m_errorBadge    = nullptr;
    QComboBox*   m_targetCombo;
    QLabel*      m_chainLabel = nullptr;
    QComboBox*   m_chainCombo = nullptr;
    QPushButton* m_chainEditBtn = nullptr;
    QPushButton* m_outputToggle;
    QPushButton* m_customizeLayoutBtn = nullptr;

    // Results pane
    QWidget*     m_resultsPane;
    QTextEdit*   m_resultsContent;
    QLabel*      m_resultsTitle;
    QPushButton* m_resultsClose;
    bool         m_resultsVisible = false;

    QWidget*     m_bar;
    QFrame*      m_barSep;
    QLabel*      m_targetLabel;
    QPushButton* m_targetHelpBtn;
    bool         m_isDark = true;
};
