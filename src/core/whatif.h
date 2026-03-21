#pragma once

#include <QDialog>
#include <QWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProcess>
#include <QString>

// ============================================================================
// WhatIfDialog — side-by-side sandbox for exploring "what if I change X to Y?"
//
// Workflow:
//   1. Right-click a line in the editor → "What If...?"
//   2. Dialog opens pre-populated with:
//        • The full line text (m_lineEdit — read-only reference)
//        • Detected token to change (m_oldValueEdit)
//        • New value to substitute  (m_newValueEdit)
//   3. "Run Original" / "Run Modified" (or both together via "Run Both")
//   4. Left pane shows original output; right pane shows modified output.
//      Differing lines are highlighted red/green.
//
// Backend:
//   - Copies the source file to a temp path.
//   - Applies a single-line text substitution (old → new) in that copy.
//   - Runs both the original and the modified copy via QProcess.
//   - Captures stdout+stderr from each.
//   - Diffs line-by-line and colour-highlights differences.
// ============================================================================
class WhatIfDialog : public QDialog {
    Q_OBJECT

public:
    // filePath    — path to the file currently open in the editor
    // lineNumber  — 1-based line that was right-clicked
    // lineText    — full text of that line
    // language    — "python", "c", "cpp", etc.  (drives the runner)
    explicit WhatIfDialog(const QString& filePath,
                          int lineNumber,
                          const QString& lineText,
                          const QString& language,
                          QWidget* parent = nullptr);
    ~WhatIfDialog() override;

    void applyTheme(bool isDark);

private slots:
    void onRunBothClicked();
    void onRunOriginalClicked();
    void onRunModifiedClicked();

private:
    // Run a file and capture output asynchronously.
    // When done, calls finishOriginal() or finishModified().
    void runFile(const QString& filePath, bool isModified);
    void finishOriginal(const QString& output);
    void finishModified(const QString& output);

    // Apply substitution: copy m_filePath to temp, replace old→new on the
    // target line, return the path to the temp file (caller must delete).
    QString buildModifiedTempFile() const;

    // Diff the two output strings line-by-line and set highlighted text
    // in both panes.
    void showDiff(const QString& original, const QString& modified);

    QString m_filePath;
    int     m_lineNumber;
    QString m_lineText;
    QString m_language;

    // Controls
    QLabel*       m_lineLabel      = nullptr;
    QLineEdit*    m_oldValueEdit   = nullptr;
    QLineEdit*    m_newValueEdit   = nullptr;
    QPushButton*  m_runBothBtn     = nullptr;
    QPushButton*  m_runOrigBtn     = nullptr;
    QPushButton*  m_runModBtn      = nullptr;

    // Output panes
    QPlainTextEdit* m_leftPane  = nullptr;   // original output
    QPlainTextEdit* m_rightPane = nullptr;   // modified output
    QLabel*         m_leftLabel = nullptr;
    QLabel*         m_rightLabel = nullptr;

    // Running processes
    QProcess* m_origProcess = nullptr;
    QProcess* m_modProcess  = nullptr;

    // Accumulated output strings (filled as processes run)
    QString m_origOutput;
    QString m_modOutput;

    // Track how many processes are still running (0, 1, or 2)
    int m_pendingRuns = 0;

    bool m_isDark = true;
};
