#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

// TTSNarrator — pipes text to a Python edge-tts worker for spoken narration.
// Buffers thinking deltas until a sentence boundary, then sends to TTS.
// Toggle on/off at runtime. No API key needed.

class TTSNarrator : public QObject {
    Q_OBJECT
public:
    explicit TTSNarrator(QObject *parent = nullptr);
    ~TTSNarrator();

    void setEnabled(bool on);
    bool isEnabled() const { return m_enabled; }

    // Feed incremental text (e.g. thinking deltas). Automatically buffers
    // until a sentence boundary (. ! ? newline) then sends to TTS.
    void feedText(const QString &text);

    // Flush any remaining buffered text to TTS.
    void flush();

    // Stop the TTS worker process.
    void stop();

signals:
    void ready();
    void error(const QString &message);

private:
    void ensureWorker();
    void sendSentence(const QString &sentence);

    QProcess *m_worker = nullptr;
    bool m_enabled = false;
    QString m_buffer;
};
