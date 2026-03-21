#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QString>
#include <QVector>

// ============================================================================
// MachineViewPanel — "What does the computer actually do?" mode
//
// A panel that sits below the editor (toggleable).
// When active, clicking any line in the editor shows a level-aware explanation
// of what the machine does for that line.
//
// Level awareness (from Assist Level):
//   Level 1 — Plain English: "A new string is made by joining two strings"
//   Level 2 — CPython/internals: "Python allocates a string object in memory..."
//   Level 3 — C-level detail: "PyObject_Malloc, memcpy, INCREF/DECREF..."
//   Level 4 — Address-level: "malloc(12) → memcpy(0x...) → ptr update"
//
// Explanations are all local text — no model required.
// Covers ~30 common code patterns for both Python and C.
// ============================================================================

// One explanation card result
struct MachineExplanation {
    QString headline;   // short title
    QString body;       // full explanation (may contain newlines)
    bool    isEmpty() const { return headline.isEmpty(); }
};

class MachineViewPanel : public QWidget {
    Q_OBJECT
public:
    explicit MachineViewPanel(QWidget* parent = nullptr);

    // Set assist level (1–4) — affects explanation verbosity
    void setLevel(int level);

    // Set language being edited ("python" / "c" / "ul" etc.)
    void setLanguage(const QString& lang);

    // Called when user clicks a line in the editor
    void explainLine(int lineNumber, const QString& lineText);

    // Clear the panel (no line selected)
    void clearExplanation();

    void applyTheme(bool isDark);

signals:
    void closeRequested();

private:
    MachineExplanation generateExplanation(const QString& line,
                                           const QString& lang,
                                           int level) const;

    // Pattern matchers — return empty explanation if pattern doesn't match
    MachineExplanation tryVariableAssignment(const QString& line, const QString& lang, int level) const;
    MachineExplanation tryStringConcat(const QString& line, const QString& lang, int level) const;
    MachineExplanation tryFunctionCall(const QString& line, const QString& lang, int level) const;
    MachineExplanation tryReturn(const QString& line, const QString& lang, int level) const;
    MachineExplanation tryIfElse(const QString& line, const QString& lang, int level) const;
    MachineExplanation tryLoop(const QString& line, const QString& lang, int level) const;
    MachineExplanation tryListOp(const QString& line, const QString& lang, int level) const;
    MachineExplanation tryImport(const QString& line, const QString& lang, int level) const;
    MachineExplanation tryPrint(const QString& line, const QString& lang, int level) const;
    MachineExplanation tryArithmetic(const QString& line, const QString& lang, int level) const;
    MachineExplanation tryCDeclaration(const QString& line, const QString& lang, int level) const;
    MachineExplanation tryCPointer(const QString& line, const QString& lang, int level) const;
    MachineExplanation tryCMalloc(const QString& line, const QString& lang, int level) const;
    MachineExplanation tryCFree(const QString& line, const QString& lang, int level) const;
    MachineExplanation tryCArray(const QString& line, const QString& lang, int level) const;

    // Widgets
    QPushButton* m_closeBtn    = nullptr;
    QLabel*      m_titleLabel  = nullptr;
    QLabel*      m_lineLabel   = nullptr;
    QLabel*      m_headLabel   = nullptr;
    QLabel*      m_bodyLabel   = nullptr;
    QLabel*      m_emptyLabel  = nullptr;

    // State
    int     m_level   = 1;
    QString m_lang    = "python";
    bool    m_isDark  = true;
};
