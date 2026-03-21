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
    void applyTheme(bool isDark);

signals:
    void runRequested();
    void buildRequested();
    void memCheckRequested();

private:
    QPushButton* m_runBtn;
    QPushButton* m_buildBtn;
    QPushButton* m_memCheckBtn;
    QComboBox*   m_targetCombo;
    QPushButton* m_outputToggle;

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
