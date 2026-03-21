#ifndef AICHAT_H
#define AICHAT_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QComboBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QVector>
#include <QSettings>
#include "core/aibackend.h"

// A single chat message bubble
class ChatBubble : public QFrame {
    Q_OBJECT
public:
    enum Role { User, AI };
    explicit ChatBubble(Role role, const QString &text, QWidget *parent = nullptr);

private:
    Role m_role;
};

// AI Chat panel with model selector and HTTP API calls
class AIChatPanel : public QWidget {
    Q_OBJECT
public:
    explicit AIChatPanel(QWidget *parent = nullptr);

    // Add a message to the chat
    void addMessage(ChatBubble::Role role, const QString &text);

    // Clear chat history
    void clearChat();

    // Set the initial welcome message for Build from Scratch mode
    void setBuildFromScratchWelcome();

    // Restore standard mode message
    void setStandardMode();

    // Get currently selected model name
    QString selectedModel() const;

    // Reload model name from saved config
    void reloadModelFromConfig();

    // Called when user opens a real file or sends their first real message
    void markExamplesSeen();

    // Set editor context for AI calls (call when active file changes)
    void setEditorContext(const QString &fileContent, const QString &language);

    // Trigger a real AI API call (called by mainwindow when not in BFS mode)
    void sendAIMessage(const QString &text);

    // Set the current assist level (1=Beginner … 4=NoAssist) for AI calls
    void setAssistLevel(int level);

    // Re-enable input after a BFS or other non-streaming response
    void setInputEnabled(bool enabled);

    // Apply light or dark theme to all inline stylesheets
    void applyTheme(bool isDark);

signals:
    void messageSent(const QString &text);
    void modelChanged(const QString &modelName);
    void closeRequested();
    // Emitted when the AI returns a code block — connects to editor to apply
    void codeBlockReceived(const QString &code, const QString &language);

private slots:
    void onSendClicked();
    void onNetworkReply(QNetworkReply *reply);
    void onConfigClicked();
    void onResponseChunk(const QString &text);
    void onCodeBlock(const QString &code, const QString &language);
    void onResponseComplete();
    void onAIError(const QString &message);

private:
    QComboBox *m_modelSelector;
    QPushButton *m_configButton;     // gear icon for model config
    QScrollArea *m_scrollArea;
    QWidget *m_messagesContainer;
    QVBoxLayout *m_messagesLayout;
    QLabel *m_placeholderLabel;      // shown when chat is empty
    QLineEdit *m_input;
    QPushButton *m_sendButton;
    QLabel *m_headerLabel;
    QWidget *m_columnHeader;
    QPushButton *m_closeButton;
    QWidget    *m_noModelBanner;  // shown when no AI model is configured
    QNetworkAccessManager *m_networkManager;
    QVector<ChatBubble*> m_bubbles;
    AIBackend *m_aiBackend;

    // Streaming AI bubble (built incrementally)
    ChatBubble  *m_streamBubble = nullptr;
    QLabel      *m_streamLabel  = nullptr;
    QString      m_streamText;

    // Current editor context
    QString m_editorContent;
    QString m_editorLanguage;

    // Current assist level for AI calls (1=Beginner, 2=Intermediate, 3=Developer, 4=NoAssist)
    int m_assistLevel = 1;

    void scrollToBottom();
    void updatePlaceholderVisibility();
    void checkModelConfigured();  // show/hide banner and enable/disable Send
    bool m_isDark = true;

    // Create a new AI bubble that can be updated while streaming
    void beginStreamBubble();
    void appendToStreamBubble(const QString &text);
    void finalizeStreamBubble();
};

#endif // AICHAT_H
