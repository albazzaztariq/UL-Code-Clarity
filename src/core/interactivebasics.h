#pragma once

#include <QDialog>
#include <QStackedWidget>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QFrame>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QJsonObject>

// ============================================================================
// InteractiveBasicsDialog
// ============================================================================
// A 15-20 minute interactive tutorial covering 7 programming topics.
// Page 0 = language selection; pages 1-7 = topic teaching + exercise(s);
// final page = completion summary.
//
// Languages: Python, C, Rust, UniLogic (checkboxes on page 0).
// Topics:
//   1. Types & Variables
//   2. Functions
//   3. If / Else Logic
//   4. Loops
//   5. Collections (arrays/lists)
//   6. Objects & Inheritance  (skippable for C-only)
//   7. Language-Specific Deep Dive
// ============================================================================

class InteractiveBasicsDialog : public QDialog {
    Q_OBJECT

public:
    explicit InteractiveBasicsDialog(QWidget *parent = nullptr);

private slots:
    void onStartClicked();
    void onCheckAnswerClicked();
    void onNextClicked();
    void onBackClicked();

private:
    // ── Language selection ────────────────────────────────────────────────
    bool m_wantPython  = true;
    bool m_wantC       = false;
    bool m_wantRust    = false;
    bool m_wantUL      = false;

    // ── Navigation state ─────────────────────────────────────────────────
    int  m_currentStep = 0;      // 0 = language select; 1-7 = topics; 8 = done
    bool m_exerciseDone = false; // must answer exercise to advance

    // Mapping from logical step index to stacked-widget page index
    // (Object step may be skipped for C-only — we just hide it)
    static const int TOTAL_STEPS = 9; // 0=lang, 1-7=topics, 8=done

    // ── Root layout ───────────────────────────────────────────────────────
    QProgressBar   *m_progressBar  = nullptr;
    QStackedWidget *m_stack        = nullptr;
    QPushButton    *m_backBtn      = nullptr;
    QPushButton    *m_nextBtn      = nullptr;

    // ── Language selection widgets ────────────────────────────────────────
    QCheckBox *m_chkPython = nullptr;
    QCheckBox *m_chkC      = nullptr;
    QCheckBox *m_chkRust   = nullptr;
    QCheckBox *m_chkUL     = nullptr;
    QPushButton *m_startBtn = nullptr;

    // ── Per-topic exercise state ──────────────────────────────────────────
    // Each topic page has one or two exercises.  We track which exercise is
    // currently active (0 or 1) within a topic, and whether it is answered.
    struct ExerciseWidgets {
        // Multiple-choice
        QVector<QRadioButton*> radios;
        QButtonGroup          *radioGroup = nullptr;
        int                    correctRadio = -1;

        // Type-answer
        QLineEdit *lineEdit = nullptr;
        QString    correctText;

        // Code-input
        QPlainTextEdit *codeEdit  = nullptr;
        QString         expectedOutput; // simple check

        // Feedback label
        QLabel      *feedbackLabel = nullptr;
        QPushButton *checkBtn      = nullptr;

        // Which exercise type: "radio", "text", "code"
        QString type;
    };

    // Topic page indices in m_stack: step 1..7 => stack page 1..7, step 8 => page 8
    // Each step may have up to 2 sequential exercises
    struct TopicExercises {
        QVector<ExerciseWidgets> exercises;
        int activeExercise = 0;
        bool topicComplete = false;
    };
    // Index 0 unused (lang page), 1-7 = topics, 8 = done
    QVector<TopicExercises> m_topicExercises;

    // ── Builder helpers ───────────────────────────────────────────────────
    void buildUI();
    void buildNavBar(QVBoxLayout *mainLayout);
    void updateProgress();
    void updateNavButtons();

    // Page builders
    QWidget* buildLangPage();
    QWidget* buildTopicPage(const QJsonObject &topic, int stepIndex);
    QWidget* buildDonePage();

    // Teaching / code block widgets
    QLabel*        makeTeachingLabel(const QString &html, QWidget *parent);
    QWidget*       makeCodeBlock(const QString &code, const QString &lang, QWidget *parent);
    QPushButton*   makeCheckButton(QWidget *parent);
    QLabel*        makeFeedbackLabel(QWidget *parent);

    // Exercise factory helpers
    ExerciseWidgets makeRadioExercise(
        QWidget *parent, QVBoxLayout *layout,
        const QString &question,
        const QStringList &options,
        int correctIndex,
        const QString &explanation);

    ExerciseWidgets makeTextExercise(
        QWidget *parent, QVBoxLayout *layout,
        const QString &question,
        const QString &correct,
        const QString &explanation);

    // Checking answers
    void flashFeedback(QLabel *label, bool correct,
                       const QString &correctMsg, const QString &wrongMsg);
};
