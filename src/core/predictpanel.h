#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>

// ============================================================================
// PredictPanel — "What do you think this will print?"
//
// Shown above the output pane when Predict mode is active.
// User types a prediction, clicks "Run & Check", sees a green/red result.
// Accuracy is tracked in QSettings and shown persistently.
//
// Layout:
//   [?] What do you think this will print?
//   [_________________________________________________] (QLineEdit)
//   [Run & Check]   Your prediction accuracy: 73% (11/15)
//   ─────────────────────────────────────────────────────
//   Result area (hidden until checked):
//     GREEN: Correct! You predicted it right.
//          — OR —
//     RED:  Not quite — actual output was [X].
//           First difference on line 2.
// ============================================================================

class PredictPanel : public QWidget {
    Q_OBJECT

public:
    explicit PredictPanel(QWidget* parent = nullptr);

    // Called by mainwindow after the run finishes with the actual output text
    void checkPrediction(const QString& actualOutput);

    // Clear prediction input and result (call before a new run)
    void reset();

    // Whether predict mode is currently active
    bool isActive() const { return m_active; }

    void applyTheme(bool isDark);

signals:
    // Emitted when user clicks "Run & Check" — mainwindow should trigger a run
    void runRequested();

    // Emitted when the panel is toggled on or off
    void activeChanged(bool active);

public slots:
    void setActive(bool active);

private slots:
    void onRunAndCheck();

private:
    void showResult(bool correct, const QString& actualOutput);
    void updateAccuracyLabel();

    // Widgets
    QLineEdit*   m_predictionInput = nullptr;
    QPushButton* m_runCheckBtn     = nullptr;
    QLabel*      m_accuracyLabel   = nullptr;
    QWidget*     m_resultArea      = nullptr;
    QLabel*      m_resultIcon      = nullptr;
    QLabel*      m_resultText      = nullptr;
    QTextEdit*   m_diffArea        = nullptr;

    bool m_active = false;
    bool m_waitingForResult = false;
    QString m_pendingPrediction;
};
