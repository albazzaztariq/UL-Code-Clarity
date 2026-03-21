#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QString>
#include <QVector>
#include <QHash>

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
// Explanations loaded from machineview.json — no model required.
// ============================================================================

struct MachineExplanation {
    QString headline;
    QString body;
    bool    isEmpty() const { return headline.isEmpty(); }
};

class MachineViewPanel : public QWidget {
    Q_OBJECT
public:
    explicit MachineViewPanel(QWidget* parent = nullptr);

    void setLevel(int level);
    void setLanguage(const QString& lang);
    void explainLine(int lineNumber, const QString& lineText);
    void clearExplanation();
    void applyTheme(bool isDark);

signals:
    void closeRequested();

private:
    // Text for one pattern at one level
    struct LevelText {
        QString headline;
        QString body;
    };

    // One loaded pattern entry from JSON
    struct PatternEntry {
        QString  id;
        QString  language;              // "" = any, "python" / "c"
        QStringList startsWithAny;
        QStringList containsAny;
        QStringList excludeStartsWith;
        QStringList excludeContains;
        QString  matchRegex;

        bool requireAssignment          = false;
        bool requireStringHint          = false;
        bool excludeStrings             = false;
        bool matchContainsStarOrAmp     = false;
        bool requireAssignOrArrow       = false;
        bool matchContainsBrackets      = false;
        bool excludeIfNoAssignAndHasParen = false;

        // levels[langKey][levelNumber] = LevelText
        QHash<QString, QHash<int, LevelText>> levels;
    };

    void loadPatterns();
    MachineExplanation generateExplanation(const QString& line,
                                           const QString& lang,
                                           int level) const;

    QVector<PatternEntry> m_patterns;

    QPushButton* m_closeBtn    = nullptr;
    QLabel*      m_titleLabel  = nullptr;
    QLabel*      m_lineLabel   = nullptr;
    QLabel*      m_headLabel   = nullptr;
    QLabel*      m_bodyLabel   = nullptr;
    QLabel*      m_emptyLabel  = nullptr;

    int     m_level   = 1;
    QString m_lang    = "python";
    bool    m_isDark  = true;
};
