#pragma once

#include <QDialog>
#include <QWidget>
#include <QCheckBox>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSettings>

// ============================================================================
// RubberDuckDialog — shown before an AI message is sent when Rubber Duck
// mode is active.
//
// Fields:
//   1. "What did you expect to happen?"
//   2. "What actually happened?"
//   3. "What have you tried so far?"
//
// Two exit paths:
//   • "Send to AI"      — emits sendWithContext(context, originalMessage)
//   • "I figured it out!" — emits solvedSelf(); closes without sending
// ============================================================================
class RubberDuckDialog : public QDialog {
    Q_OBJECT

public:
    explicit RubberDuckDialog(const QString& originalMessage,
                              QWidget* parent = nullptr);

    void applyTheme(bool isDark);

signals:
    // All three answers prepended to the original message, ready to send.
    void sendWithContext(const QString& contextualMessage);
    // User realised the answer themselves — don't send anything.
    void solvedSelf();

private slots:
    void onSendClicked();
    void onSolvedClicked();

private:
    QString      m_originalMessage;
    QTextEdit*   m_expectField  = nullptr;
    QTextEdit*   m_actualField  = nullptr;
    QTextEdit*   m_triedField   = nullptr;
    QPushButton* m_sendBtn      = nullptr;
    QPushButton* m_solvedBtn    = nullptr;
};


// ============================================================================
// RubberDuckToggle — a small checkbox+label widget for the AI Chat header.
// When isEnabled() returns true, the chat panel intercepts Send and opens
// the RubberDuckDialog instead of firing the message immediately.
//
// Self-solve count is persisted in QSettings("CodeClarity","RubberDuck").
// ============================================================================
class RubberDuckToggle : public QWidget {
    Q_OBJECT

public:
    explicit RubberDuckToggle(QWidget* parent = nullptr);

    bool isEnabled() const;

    // Call to increment and display the self-solve counter.
    static void incrementSelfSolveCount();
    static int  selfSolveCount();

    void applyTheme(bool isDark);

signals:
    void toggled(bool enabled);

private:
    QCheckBox* m_check  = nullptr;
    QLabel*    m_label  = nullptr;
    bool       m_isDark = true;

    void updateSolveLabel();
};
