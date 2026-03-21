#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// AIBackend — handles streaming AI API calls for Anthropic, OpenAI, and Ollama.
//
// Reads provider/endpoint/apiKey/modelName from QSettings("CodeClarity","CodeClarity")
// under keys: ai/provider, ai/endpoint, ai/apiKey, ai/modelName
//
// Signals:
//   responseChunk(text)  — emitted for each streamed text fragment
//   codeBlock(code, lang) — emitted when a complete fenced code block is detected
//   responseComplete()   — emitted when the full response is done
//   errorOccurred(msg)   — emitted on network or API error

class AIBackend : public QObject {
    Q_OBJECT

public:
    explicit AIBackend(QObject *parent = nullptr);

    // Send a message to the configured AI provider.
    // userMessage  — the user's chat text
    // fileContent  — current editor content (empty string if no file open)
    // language     — language name, e.g. "python", "c", "unilogic"
    // level        — assist level 1=Beginner, 2=Intermediate, 3=Developer, 4=NoAssist
    void sendMessage(const QString &userMessage,
                     const QString &fileContent,
                     const QString &language,
                     int level = 1);

    // Cancel any in-flight request
    void abort();

signals:
    void responseChunk(const QString &text);
    void codeBlock(const QString &code, const QString &language);
    void responseComplete();
    void errorOccurred(const QString &message);

private slots:
    void onReadyRead();
    void onFinished();

private:
    QNetworkAccessManager *m_nam;
    QNetworkReply         *m_reply = nullptr;

    // Accumulated response buffer for parsing streaming data
    QString m_buffer;
    // Partial accumulated response for code block detection
    QString m_fullResponse;

    // Helpers
    QString buildSystemPrompt(const QString &language, const QString &fileContent, int level) const;
    static QString levelSystemPrompt(int level);

    // Build request body JSON per provider
    QByteArray buildAnthropicBody(const QString &systemPrompt,
                                  const QString &userMessage,
                                  const QString &model) const;
    QByteArray buildOpenAIBody(const QString &systemPrompt,
                               const QString &userMessage,
                               const QString &model) const;
    QByteArray buildOllamaBody(const QString &systemPrompt,
                               const QString &userMessage,
                               const QString &model) const;

    // Parse a single SSE data line and return the text delta (empty if not applicable)
    QString parseAnthropicSSE(const QString &line) const;
    QString parseOpenAISSE(const QString &line) const;
    QString parseOllamaChunk(const QString &line) const;

    // Scan m_fullResponse for completed fenced code blocks not yet emitted
    void detectCodeBlocks();

    QString m_provider;
    int m_emittedCodeBlocks = 0; // how many code blocks we've already emitted
};
