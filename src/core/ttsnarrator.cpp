#include "core/ttsnarrator.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QDebug>

TTSNarrator::TTSNarrator(QObject *parent)
    : QObject(parent)
{
}

TTSNarrator::~TTSNarrator()
{
    stop();
}

void TTSNarrator::setEnabled(bool on)
{
    m_enabled = on;
    if (on)
        ensureWorker();
    else
        stop();
}

void TTSNarrator::ensureWorker()
{
    if (m_worker && m_worker->state() == QProcess::Running)
        return;

    // Find the tts_worker.py script next to the executable in data/
    QString exeDir = QCoreApplication::applicationDirPath();
    QString script = exeDir + "/data/tts_worker.py";

    if (!QFile::exists(script)) {
        emit error("TTS worker script not found: " + script);
        return;
    }

    // Find Python — try common locations
    QStringList pythonCandidates = {
        "python", "python3", "pythonw",
        QDir::homePath() + "/AppData/Local/Programs/Python/Python312/python.exe",
        QDir::homePath() + "/AppData/Local/Programs/Python/Python311/python.exe",
    };

    QString pythonExe;
    for (const QString &candidate : pythonCandidates) {
        QProcess test;
        test.start(candidate, {"--version"});
        if (test.waitForFinished(2000) && test.exitCode() == 0) {
            pythonExe = candidate;
            break;
        }
    }

    if (pythonExe.isEmpty()) {
        emit error("Python not found — TTS narration requires Python with edge-tts");
        return;
    }

    m_worker = new QProcess(this);
    connect(m_worker, &QProcess::readyReadStandardOutput, this, [this]() {
        QString out = QString::fromUtf8(m_worker->readAllStandardOutput()).trimmed();
        if (out == "TTS_READY")
            emit ready();
    });
    connect(m_worker, &QProcess::readyReadStandardError, this, [this]() {
        QString err = QString::fromUtf8(m_worker->readAllStandardError()).trimmed();
        if (!err.isEmpty())
            qWarning() << "[TTS]" << err;
    });

    qDebug() << "[TTS] Starting worker:" << pythonExe << script;
    m_worker->start(pythonExe, {script});

    if (!m_worker->waitForStarted(5000)) {
        emit error("Failed to start TTS worker: " + m_worker->errorString());
        m_worker->deleteLater();
        m_worker = nullptr;
    }
}

void TTSNarrator::feedText(const QString &text)
{
    if (!m_enabled) return;

    m_buffer += text;

    // Send complete sentences to TTS
    static const QRegularExpression sentenceEnd("[.!?\\n]");
    int pos;
    while ((pos = m_buffer.indexOf(sentenceEnd)) >= 0) {
        QString sentence = m_buffer.left(pos + 1).trimmed();
        m_buffer = m_buffer.mid(pos + 1);
        if (!sentence.isEmpty())
            sendSentence(sentence);
    }
}

void TTSNarrator::flush()
{
    if (!m_enabled || m_buffer.trimmed().isEmpty()) return;
    sendSentence(m_buffer.trimmed());
    m_buffer.clear();
}

void TTSNarrator::sendSentence(const QString &sentence)
{
    ensureWorker();
    if (!m_worker || m_worker->state() != QProcess::Running) return;

    // Skip very short fragments and code-like text
    if (sentence.length() < 10) return;
    if (sentence.contains('{') || sentence.contains('}')) return;
    if (sentence.startsWith("```") || sentence.startsWith("//")) return;

    m_worker->write((sentence + "\n").toUtf8());
}

void TTSNarrator::stop()
{
    if (!m_worker) return;
    if (m_worker->state() == QProcess::Running) {
        m_worker->write("QUIT\n");
        if (!m_worker->waitForFinished(3000))
            m_worker->kill();
    }
    m_worker->deleteLater();
    m_worker = nullptr;
    m_buffer.clear();
}
