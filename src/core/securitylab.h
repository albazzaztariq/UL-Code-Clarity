#pragma once

#include <QWidget>
#include <QString>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QProcess>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QList>

// ============================================================================
// LabDefinition — data describing one interactive security lab
// ============================================================================
struct LabDefinition {
    QString name;           // Display name, e.g. "Buffer Overflow"
    QString vulnType;       // Key used to match SAST findings: "buffer_overflow", "sql_injection", etc.
    QString vulnCode;       // Code shown in the vulnerable code pane
    QString fixedCode;      // Code shown after "Show the Fix" is pressed
    QString normalInput;    // Pre-filled normal (safe) input
    QString attackInput;    // Pre-filled attack input
    QString explanation;    // Shown when "Why This Matters" is pressed
    QString fixExplanation; // Shown below the fixed code
    QString labProgram;     // Absolute path to the lab executable / script to run
    QString labProgram2;    // Alternate program for fixed version (optional)
    bool    isPython;       // true → run with python3/python; false → compile & run C
};

// ============================================================================
// SecurityLabWidget — interactive vulnerability lab
//
// Layout (3-pane QSplitter):
//   LEFT  — Code pane (read-only QPlainTextEdit)
//   CENTER— Input + controls
//   RIGHT — Output / shell pane (QPlainTextEdit, read-only)
//
// Buttons:
//   "Run Normal Input"   — feed normalInput to the lab program
//   "Run Attack Input"   — feed attackInput to the lab program
//   "Show the Fix"       — swap code pane to fixedCode
//   "Why This Matters"   — show explanation dialog
//
// SQL and XSS labs also show a styled "browser/login" mock UI on top
// of the input area — styled QWidgets that visually simulate a webpage.
// ============================================================================
class SecurityLabWidget : public QWidget {
    Q_OBJECT

public:
    explicit SecurityLabWidget(const LabDefinition& lab, QWidget* parent = nullptr);

    // Factory — look up a built-in lab by vulnType key.
    // Returns nullptr if no lab exists for that type.
    static SecurityLabWidget* forVulnType(const QString& vulnType, QWidget* parent = nullptr);

    // All built-in lab definitions (used by Tools > Security Labs browser)
    static QList<LabDefinition> allLabs();

signals:
    void closeRequested();

private slots:
    void runNormal();
    void runAttack();
    void showFix();
    void showWhyItMatters();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError err);

private:
    void runWithInput(const QString& input);
    void appendOutput(const QString& text, const QString& color = "#cdd6f4");
    void buildSqlMockUi(QWidget* parent, QVBoxLayout* layout);
    void buildXssMockUi(QWidget* parent, QVBoxLayout* layout);

    LabDefinition        m_lab;
    bool                 m_showingFix  = false;

    // ── Widgets ──────────────────────────────────────────────────────────────
    QPlainTextEdit*  m_codePaneVuln  = nullptr;   // vulnerable code
    QPlainTextEdit*  m_codePaneFix   = nullptr;   // fixed code (hidden until showFix)
    QStackedWidget*  m_codeStack     = nullptr;

    QLineEdit*       m_inputField    = nullptr;   // general input field
    QTextEdit*       m_outputPane    = nullptr;   // shell output

    QPushButton*     m_runNormalBtn  = nullptr;
    QPushButton*     m_runAttackBtn  = nullptr;
    QPushButton*     m_fixBtn        = nullptr;
    QPushButton*     m_whyBtn        = nullptr;

    QLabel*          m_statusLabel   = nullptr;

    // SQL mock UI
    QLineEdit*       m_sqlUsername   = nullptr;
    QLineEdit*       m_sqlPassword   = nullptr;

    // XSS mock UI
    QTextEdit*       m_xssComment    = nullptr;

    QProcess*        m_process       = nullptr;
    QString          m_pendingInput;
};
