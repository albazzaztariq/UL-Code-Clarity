#pragma once

#include <QObject>
#include <QStringList>
#include <QProcess>
#include <QTimer>

// ── CVEMonitor ───────────────────────────────────────────────────────────────
// Runs "pip audit --format json" on demand or on a configurable schedule.
// Does NOT run automatically on startup — must be enabled by the user.
//
// Usage:
//   monitor->setWorkspaceRoot(root);
//   monitor->setEnabled(true);            // starts the schedule
//   monitor->setFrequencyMs(3600000);     // every hour
//   monitor->runNow();                    // one-shot manual trigger
//
// Signals:
//   vulnerabilitiesFound(count, packages) — emitted when pip-audit finds CVEs
//   auditCompleted(count, packages)       — emitted after every run (0 if clean)
class CVEMonitor : public QObject {
    Q_OBJECT
public:
    // Frequency presets (milliseconds)
    enum Frequency {
        Every5Min  = 5   * 60 * 1000,
        Every30Min = 30  * 60 * 1000,
        Hourly     = 60  * 60 * 1000,
        Daily      = 24  * 60 * 60 * 1000,
        Weekly     = 7   * 24 * 60 * 60 * 1000,
    };

    explicit CVEMonitor(QObject* parent = nullptr);

    // Configure workspace root (required before running)
    void setWorkspaceRoot(const QString& root);

    // Enable/disable scheduled monitoring
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    // Set polling interval in milliseconds (use Frequency enum or custom value)
    void setFrequencyMs(int ms);
    int frequencyMs() const { return m_frequencyMs; }

    // Run one audit immediately (regardless of schedule)
    void runNow();

    // Convenience: load/save user preference from QSettings
    static bool loadEnabled();
    static int  loadFrequencyMs();
    static void saveEnabled(bool enabled);
    static void saveFrequencyMs(int ms);

signals:
    // Emitted only when vulnerabilities are found
    void vulnerabilitiesFound(int count, const QStringList& packages);
    // Emitted after every completed run (count=0 if clean)
    void auditCompleted(int count, const QStringList& packages);

private slots:
    void onTimerFired();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    bool hasPythonFiles(const QString& root) const;
    void startAudit();
    void parseOutput(const QByteArray& output);

    QTimer*   m_timer       = nullptr;
    QProcess* m_process     = nullptr;
    QString   m_root;
    bool      m_enabled     = false;
    int       m_frequencyMs = Hourly;
};
