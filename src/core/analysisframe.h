#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>

// Base class for all analysis tool panels.
// Provides: back button, centred title, help button, results scroll area, status bar.
// Subclasses implement:
//   buildControls() — return a QWidget* to insert between the header and results
//   runAnalysis()   — fill the results scroll area and update m_statusLabel
class AnalysisFrame : public QWidget
{
    Q_OBJECT

public:
    explicit AnalysisFrame(const QString& title, QWidget* parent = nullptr);

    // Called by mainwindow to supply the current code and language before showing the frame.
    virtual void setCode(const QString& code, const QString& language);

signals:
    void backToEditor();
    void jumpToLine(const QString& filePath, int lineNumber);

protected slots:
    void onHelpClicked();

protected:
    // Subclasses override these.
    // buildControls: return an optional control bar widget (or nullptr).
    virtual QWidget* buildControls() { return nullptr; }
    // runAnalysis: perform analysis and populate m_resultsWidget.
    virtual void runAnalysis() {}
    // onHelp: subclasses override to show a help/tutorial dialog.
    virtual void showHelp() {}

    // Helpers available to subclasses.
    void clearResults();
    void addResultWidget(QWidget* w);
    void setStatus(const QString& text);

    QString m_code;
    QString m_language;

    QLabel*      m_statusLabel  { nullptr };
    QVBoxLayout* m_resultsLayout{ nullptr };

private:
    void buildUi(const QString& title);

    QScrollArea* m_scrollArea   { nullptr };
    QWidget*     m_resultsWidget{ nullptr };
};
