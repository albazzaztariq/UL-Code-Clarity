#pragma once

#include <QObject>
#include <QDialog>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QScrollArea>

// ============================================================================
// BuildChainStep — one step in a build chain
// ============================================================================
struct BuildChainStep {
    QString id;       // "compile", "memcheck", "security", "fullreport", "depcheck", "run"
    QString label;    // human-readable
    bool    enabled;  // whether this step is active
};

// ============================================================================
// BuildChainConfig — a named ordered list of steps
// ============================================================================
struct BuildChainConfig {
    QString                  name;
    QVector<BuildChainStep>  steps;

    // QSettings persistence
    void save() const;
    static BuildChainConfig load(const QString& name);
    static BuildChainConfig defaultConfig();
    static QStringList      savedChainNames();
    static void             remove(const QString& name);
};

// ============================================================================
// BuildChainRunner — runs a chain sequentially, emitting step results
// ============================================================================
class BuildChainRunner : public QObject {
    Q_OBJECT

public:
    explicit BuildChainRunner(const BuildChainConfig& config, QObject* parent = nullptr);

    // lang: "python", "c", "cpp", etc.
    void run(const QString& filePath, const QString& lang);

signals:
    void stepStarted(const QString& stepId, const QString& label);
    void stepFinished(const QString& stepId, bool passed, const QString& output);
    void chainFinished(bool allPassed);

private:
    void runNextStep();
    bool runStep(const BuildChainStep& step,
                 const QString& filePath,
                 const QString& lang,
                 QString& outText);

    BuildChainConfig m_config;
    QString          m_filePath;
    QString          m_lang;
    int              m_stepIndex = 0;
};

// ============================================================================
// BuildChainDialog — UI to create/edit a build chain
// ============================================================================
class BuildChainDialog : public QDialog {
    Q_OBJECT

public:
    explicit BuildChainDialog(QWidget* parent = nullptr);

    // Load an existing chain for editing
    void loadChain(const BuildChainConfig& config);

    // Return the current chain as configured in the dialog
    BuildChainConfig currentConfig() const;

signals:
    void chainSaved(const QString& name);

private slots:
    void onSave();
    void onMoveUp();
    void onMoveDown();

private:
    void rebuildStepList();
    void updateButtonStates();

    struct StepRow {
        BuildChainStep step;
        QWidget*       widget    = nullptr;
        QPushButton*   upBtn     = nullptr;
        QPushButton*   downBtn   = nullptr;
        QPushButton*   toggleBtn = nullptr;
        QLabel*        label     = nullptr;
    };

    QVector<StepRow> m_rows;
    QWidget*         m_stepsContainer = nullptr;
    QVBoxLayout*     m_stepsLayout    = nullptr;
    QLineEdit*       m_nameEdit       = nullptr;
    int              m_selectedRow    = -1;
};
