#pragma once
// custompipeline.h — types now defined here; implementation merged into buildchain.cpp

#include <QDialog>
#include <QWidget>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QCheckBox>
#include <QMap>

// ── PipelineConfig ────────────────────────────────────────────────────────
struct PipelineConfig {
    QString     name;
    QStringList checks;

    static QStringList availableChecks();

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
