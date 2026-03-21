#pragma once

#include <QDialog>
#include <QWidget>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QCheckBox>
#include <QMap>

// ── PipelineConfig ────────────────────────────────────────────────────────
// Holds the list of check names that belong to a saved pipeline.
struct PipelineConfig {
    QString     name;
    QStringList checks;  // e.g. {"Security", "Code Quality", "Memory", "Dependencies", "Runtime"}

    // Available check IDs
    static QStringList availableChecks();

    // Persistence — stored in QSettings under "pipelines/<name>"
    void save() const;
    static PipelineConfig load(const QString& name);
    static QStringList savedPipelineNames();
    static void remove(const QString& name);
};

// ── PipelineResult ────────────────────────────────────────────────────────
struct PipelineResult {
    QString checkName;
    bool    passed  = true;
    QString summary;
};

// ── PipelineRunner ────────────────────────────────────────────────────────
// Runs each selected check in sequence against the provided code/file.
// Emits finished() when all checks have run.
class PipelineRunner : public QObject {
    Q_OBJECT
public:
    explicit PipelineRunner(const PipelineConfig& cfg, QObject* parent = nullptr);

    void run(const QString& code, const QString& lang, const QString& filePath);

signals:
    void finished(const QList<PipelineResult>& results);

private:
    PipelineConfig m_cfg;

    PipelineResult runSecurityCheck(const QString& code, const QString& lang,
                                    const QString& filePath) const;
    PipelineResult runCodeQualityCheck(const QString& code, const QString& lang) const;
    PipelineResult runMemoryCheck(const QString& code, const QString& lang) const;
    PipelineResult runDependencyCheck(const QString& code, const QString& lang,
                                      const QString& filePath) const;
    PipelineResult runRuntimeCheck(const QString& code, const QString& lang) const;
};

// ── CustomPipelineDialog ─────────────────────────────────────────────────
// Dialog with checkboxes for each available analysis.
// "Save Pipeline" saves config. "Run Now" runs immediately.
class CustomPipelineDialog : public QDialog {
    Q_OBJECT
public:
    explicit CustomPipelineDialog(QWidget* parent = nullptr);

signals:
    void pipelineSaved();
    void runRequested(const PipelineConfig& cfg);

private slots:
    void onSave();
    void onRun();
    void onLoad();

private:
    PipelineConfig currentConfig() const;

    QMap<QString, QCheckBox*> m_checkBoxes;
    class QLineEdit* m_nameEdit = nullptr;
};
