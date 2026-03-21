#include "core/cvemonitor.h"

#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>

CVEMonitor::CVEMonitor(QObject* parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setSingleShot(false);
    connect(m_timer, &QTimer::timeout, this, &CVEMonitor::onTimerFired);
}

// ── Configuration ─────────────────────────────────────────────────────────────

void CVEMonitor::setWorkspaceRoot(const QString& root)
{
    m_root = root;
}

void CVEMonitor::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (enabled) {
        m_timer->start(m_frequencyMs);
    } else {
        m_timer->stop();
        // Terminate any in-progress audit
        if (m_process && m_process->state() != QProcess::NotRunning) {
            m_process->kill();
        }
    }
}

void CVEMonitor::setFrequencyMs(int ms)
{
    m_frequencyMs = qMax(ms, 60 * 1000); // minimum 1 minute
    if (m_timer->isActive())
        m_timer->setInterval(m_frequencyMs);
}

void CVEMonitor::runNow()
{
    if (m_process && m_process->state() != QProcess::NotRunning)
        return; // already running
    startAudit();
}

// ── QSettings persistence ────────────────────────────────────────────────────

bool CVEMonitor::loadEnabled()
{
    QSettings s("CodeClarity", "CodeClarity");
    return s.value("cve/enabled", false).toBool();
}

int CVEMonitor::loadFrequencyMs()
{
    QSettings s("CodeClarity", "CodeClarity");
    return s.value("cve/frequencyMs", static_cast<int>(Hourly)).toInt();
}

void CVEMonitor::saveEnabled(bool enabled)
{
    QSettings s("CodeClarity", "CodeClarity");
    s.setValue("cve/enabled", enabled);
}

void CVEMonitor::saveFrequencyMs(int ms)
{
    QSettings s("CodeClarity", "CodeClarity");
    s.setValue("cve/frequencyMs", ms);
}

// ── Internal audit logic ──────────────────────────────────────────────────────

void CVEMonitor::onTimerFired()
{
    startAudit();
}

void CVEMonitor::startAudit()
{
    if (m_root.isEmpty() || !hasPythonFiles(m_root))
        return;

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->setWorkingDirectory(m_root);

    connect(m_process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &CVEMonitor::onProcessFinished);

    m_process->start("pip", {"audit", "--format", "json"});
}

bool CVEMonitor::hasPythonFiles(const QString& root) const
{
    QDirIterator it(root, {"*.py"}, QDir::Files,
                    QDirIterator::Subdirectories);
    return it.hasNext();
}

void CVEMonitor::onProcessFinished(int exitCode, QProcess::ExitStatus /*status*/)
{
    if (!m_process) return;
    QByteArray output = m_process->readAll();
    m_process->deleteLater();
    m_process = nullptr;

    if (exitCode == 0) {
        // No vulnerabilities
        emit auditCompleted(0, {});
        return;
    }

    if (!output.isEmpty()) {
        parseOutput(output);
    }
}

void CVEMonitor::parseOutput(const QByteArray& output)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(output, &err);

    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        emit auditCompleted(0, {});
        return;
    }

    QJsonArray arr = doc.array();
    QStringList affectedPackages;
    int vulnCount = 0;

    for (const QJsonValue& val : arr) {
        QJsonObject pkg = val.toObject();
        QJsonArray vulns = pkg["vulns"].toArray();
        if (!vulns.isEmpty()) {
            affectedPackages.append(pkg["name"].toString());
            vulnCount += vulns.size();
        }
    }

    emit auditCompleted(vulnCount, affectedPackages);
    if (vulnCount > 0 && !affectedPackages.isEmpty()) {
        emit vulnerabilitiesFound(vulnCount, affectedPackages);
    }
}
