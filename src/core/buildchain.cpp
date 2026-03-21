#include "core/buildchain.h"

#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QFrame>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>

// ── Default step definitions ─────────────────────────────────────────────────
static QVector<BuildChainStep> defaultSteps()
{
    return {
        { "compile",    "Compile",            true  },
        { "memcheck",   "Memory Check",       false },
        { "security",   "Security Scan",      false },
        { "fullreport", "Full Report + Score",false  },
        { "depcheck",   "Dependency Check",   false },
        { "run",        "Run",                true  },
    };
}

// ── BuildChainConfig ─────────────────────────────────────────────────────────
BuildChainConfig BuildChainConfig::defaultConfig()
{
    BuildChainConfig cfg;
    cfg.name  = "Default";
    cfg.steps = defaultSteps();
    return cfg;
}

void BuildChainConfig::save() const
{
    QSettings s("CodeClarity", "CodeClarity");
    s.beginGroup("buildchains/" + name);
    s.setValue("name", name);
    int count = steps.size();
    s.setValue("count", count);
    for (int i = 0; i < count; ++i) {
        s.setValue(QString("step%1/id").arg(i),      steps[i].id);
        s.setValue(QString("step%1/label").arg(i),   steps[i].label);
        s.setValue(QString("step%1/enabled").arg(i), steps[i].enabled);
    }
    s.endGroup();

    // Add to the list of saved chain names
    QStringList names = savedChainNames();
    if (!names.contains(name)) {
        names.append(name);
        s.setValue("buildchains/_names", names);
    }
}

BuildChainConfig BuildChainConfig::load(const QString& name)
{
    QSettings s("CodeClarity", "CodeClarity");
    s.beginGroup("buildchains/" + name);

    BuildChainConfig cfg;
    cfg.name = s.value("name", name).toString();
    int count = s.value("count", 0).toInt();
    for (int i = 0; i < count; ++i) {
        BuildChainStep step;
        step.id      = s.value(QString("step%1/id").arg(i)).toString();
        step.label   = s.value(QString("step%1/label").arg(i)).toString();
        step.enabled = s.value(QString("step%1/enabled").arg(i), false).toBool();
        cfg.steps.append(step);
    }
    s.endGroup();

    if (cfg.steps.isEmpty())
        cfg.steps = defaultSteps();

    return cfg;
}

QStringList BuildChainConfig::savedChainNames()
{
    QSettings s("CodeClarity", "CodeClarity");
    return s.value("buildchains/_names").toStringList();
}

void BuildChainConfig::remove(const QString& name)
{
    QSettings s("CodeClarity", "CodeClarity");
    s.remove("buildchains/" + name);
    QStringList names = savedChainNames();
    names.removeAll(name);
    s.setValue("buildchains/_names", names);
}

// ── BuildChainRunner ─────────────────────────────────────────────────────────
BuildChainRunner::BuildChainRunner(const BuildChainConfig& config, QObject* parent)
    : QObject(parent), m_config(config)
{
}

void BuildChainRunner::run(const QString& filePath, const QString& lang)
{
    m_filePath  = filePath;
    m_lang      = lang;
    m_stepIndex = 0;
    runNextStep();
}

void BuildChainRunner::runNextStep()
{
    // Skip disabled steps
    while (m_stepIndex < m_config.steps.size() && !m_config.steps[m_stepIndex].enabled)
        ++m_stepIndex;

    if (m_stepIndex >= m_config.steps.size()) {
        emit chainFinished(true);
        return;
    }

    const BuildChainStep& step = m_config.steps[m_stepIndex];

    // For Python: rename "compile" → "syntax check"
    QString effectiveLabel = step.label;
    if (step.id == "compile" && m_lang == "python")
        effectiveLabel = "Syntax Check";
    if (step.id == "memcheck" && m_lang == "python") {
        // Skip memory check for Python silently
        ++m_stepIndex;
        runNextStep();
        return;
    }

    emit stepStarted(step.id, effectiveLabel);

    QString output;
    bool passed = runStep(step, m_filePath, m_lang, output);

    emit stepFinished(step.id, passed, output);

    if (!passed) {
        emit chainFinished(false);
        return;
    }

    ++m_stepIndex;
    runNextStep();
}

bool BuildChainRunner::runStep(const BuildChainStep& step,
                                const QString& filePath,
                                const QString& lang,
                                QString& outText)
{
    QFileInfo fi(filePath);
    QString dir = fi.absolutePath();

    if (step.id == "compile") {
        if (lang == "python") {
            // python -m py_compile file.py
            QProcess p;
            p.setWorkingDirectory(dir);
            p.start("python", {"-m", "py_compile", filePath});
            p.waitForFinished(15000);
            QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
            outText = err.isEmpty() ? "Syntax OK" : err;
            return p.exitCode() == 0;
        } else if (lang == "c" || lang == "cpp") {
            QString compiler = (lang == "cpp") ? "g++" : "gcc";
            QString outBin = dir + "/cc_build_output";
            QProcess p;
            p.setWorkingDirectory(dir);
            p.start(compiler, {filePath, "-o", outBin, "-Wall"});
            p.waitForFinished(30000);
            QString out = QString::fromUtf8(p.readAllStandardOutput()) +
                          QString::fromUtf8(p.readAllStandardError());
            outText = out.trimmed().isEmpty() ? "Compiled OK" : out.trimmed();
            return p.exitCode() == 0;
        }
        outText = "Compile step not supported for: " + lang;
        return true; // treat as pass for unsupported languages
    }

    if (step.id == "memcheck") {
        // Run valgrind if available
        QProcess p;
        p.setWorkingDirectory(dir);
        p.start("valgrind", {"--error-exitcode=1", filePath});
        if (!p.waitForStarted(3000)) {
            outText = "valgrind not found — skipped";
            return true;
        }
        p.waitForFinished(30000);
        QString out = QString::fromUtf8(p.readAllStandardError()).trimmed();
        outText = out.isEmpty() ? "No memory errors detected" : out;
        return p.exitCode() == 0;
    }

    if (step.id == "security") {
        // Run bandit for Python, cppcheck for C/C++
        QProcess p;
        p.setWorkingDirectory(dir);
        if (lang == "python") {
            p.start("bandit", {"-r", filePath, "-ll"});
        } else {
            p.start("cppcheck", {"--error-exitcode=1", filePath});
        }
        if (!p.waitForStarted(3000)) {
            outText = "Security scanner not found — skipped";
            return true;
        }
        p.waitForFinished(30000);
        QString out = (QString::fromUtf8(p.readAllStandardOutput()) +
                       QString::fromUtf8(p.readAllStandardError())).trimmed();
        outText = out.isEmpty() ? "No security issues found" : out;
        return p.exitCode() == 0;
    }

    if (step.id == "fullreport") {
        // Simple local analysis: count lines, check for obvious issues
        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            outText = "Cannot open file for analysis";
            return false;
        }
        QString code = QString::fromUtf8(f.readAll());
        f.close();
        int lines = code.count('\n') + 1;
        int score = 100;
        QStringList issues;
        if (lines > 500) { issues << "File is large (" + QString::number(lines) + " lines)"; score -= 10; }
        if (code.contains("TODO") || code.contains("FIXME")) { issues << "Contains TODO/FIXME"; score -= 5; }
        if (lang == "python" && code.contains("except:")) { issues << "Bare except clause found"; score -= 15; }
        if (lang == "c" && code.contains("gets(")) { issues << "Unsafe gets() call"; score -= 20; }
        outText = QString("Score: %1/100\n").arg(score);
        if (issues.isEmpty()) outText += "No issues found.";
        else outText += "Issues:\n  " + issues.join("\n  ");
        return score >= 60;
    }

    if (step.id == "depcheck") {
        if (lang == "python") {
            // Check imports vs installed packages
            QProcess p;
            p.setWorkingDirectory(dir);
            p.start("python", {"-c",
                QString("import ast, sys; "
                        "tree = ast.parse(open('%1').read()); "
                        "imports = [n.names[0].name for n in ast.walk(tree) if isinstance(n, ast.Import)]; "
                        "print('Dependencies:', imports)").arg(filePath)});
            p.waitForFinished(10000);
            outText = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
            if (outText.isEmpty()) outText = "No imports detected";
            return true;
        }
        outText = "Dependency check not available for: " + lang;
        return true;
    }

    if (step.id == "run") {
        QProcess p;
        p.setWorkingDirectory(dir);
        if (lang == "python") {
            p.start("python", {filePath});
        } else if (lang == "c" || lang == "cpp") {
            QString outBin = dir + "/cc_build_output";
            if (!QFileInfo::exists(outBin)) {
                outText = "No compiled binary found — did compile step run?";
                return false;
            }
            p.start(outBin);
        } else {
            outText = "Run not supported for: " + lang;
            return true;
        }
        if (!p.waitForStarted(5000)) {
            outText = "Failed to start process";
            return false;
        }
        p.waitForFinished(30000);
        outText = (QString::fromUtf8(p.readAllStandardOutput()) +
                   QString::fromUtf8(p.readAllStandardError())).trimmed();
        if (outText.isEmpty()) outText = "(no output)";
        return p.exitCode() == 0;
    }

    outText = "Unknown step: " + step.id;
    return false;
}

// ── BuildChainDialog ─────────────────────────────────────────────────────────
BuildChainDialog::BuildChainDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Build Chain Editor");
    setMinimumSize(420, 480);
    setStyleSheet(
        "QDialog { background: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; }"
        "QLineEdit { background: #2a2a3c; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; padding: 4px 8px; font-size: 12px; }"
        "QPushButton { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; padding: 4px 12px; font-size: 12px; }"
        "QPushButton:hover { background: #45475a; }"
        "QPushButton:disabled { color: #585b70; border-color: #313244; }");

    auto* outerLay = new QVBoxLayout(this);
    outerLay->setContentsMargins(16, 14, 16, 14);
    outerLay->setSpacing(10);

    // Title
    auto* titleLbl = new QLabel("Build Chain");
    titleLbl->setStyleSheet("font-size: 15px; font-weight: bold; color: #cdd6f4;");
    outerLay->addWidget(titleLbl);

    auto* subLbl = new QLabel(
        "Define the sequence of steps run when you click Build.\n"
        "Enable/disable steps with the checkbox, reorder with the arrows.");
    subLbl->setWordWrap(true);
    subLbl->setStyleSheet("color: #a6adc8; font-size: 11px;");
    outerLay->addWidget(subLbl);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("QFrame { background: #313244; }");
    outerLay->addWidget(sep);

    // Chain name
    auto* nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel("Chain Name:"));
    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText("e.g. My Python Chain");
    nameRow->addWidget(m_nameEdit, 1);
    outerLay->addLayout(nameRow);

    // Steps list
    m_stepsContainer = new QWidget;
    m_stepsLayout    = new QVBoxLayout(m_stepsContainer);
    m_stepsLayout->setContentsMargins(0, 0, 0, 0);
    m_stepsLayout->setSpacing(4);

    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet(
        "QScrollArea { border: 1px solid #313244; border-radius: 4px; background: #2a2a3c; }");
    scrollArea->setWidget(m_stepsContainer);
    outerLay->addWidget(scrollArea, 1);

    // Button row
    auto* btnRow = new QHBoxLayout;
    auto* saveBtn   = new QPushButton("Save Chain");
    auto* cancelBtn = new QPushButton("Cancel");
    btnRow->addStretch();
    btnRow->addWidget(saveBtn);
    btnRow->addWidget(cancelBtn);
    outerLay->addLayout(btnRow);

    connect(saveBtn,   &QPushButton::clicked, this, &BuildChainDialog::onSave);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    // Load default
    loadChain(BuildChainConfig::defaultConfig());
}

void BuildChainDialog::loadChain(const BuildChainConfig& config)
{
    m_nameEdit->setText(config.name);
    m_rows.clear();
    for (const auto& step : config.steps) {
        StepRow row;
        row.step = step;
        m_rows.append(row);
    }
    rebuildStepList();
}

BuildChainConfig BuildChainDialog::currentConfig() const
{
    BuildChainConfig cfg;
    cfg.name = m_nameEdit->text().trimmed();
    if (cfg.name.isEmpty()) cfg.name = "Unnamed";
    for (const auto& row : m_rows)
        cfg.steps.append(row.step);
    return cfg;
}

void BuildChainDialog::rebuildStepList()
{
    // Clear existing widgets
    while (m_stepsLayout->count() > 0) {
        auto* item = m_stepsLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    // Clear widget pointers (step data preserved in m_rows[].step)
    for (auto& row : m_rows) {
        row.widget    = nullptr;
        row.upBtn     = nullptr;
        row.downBtn   = nullptr;
        row.toggleBtn = nullptr;
        row.label     = nullptr;
    }

    for (int i = 0; i < m_rows.size(); ++i) {
        StepRow& row = m_rows[i];

        row.widget = new QWidget(m_stepsContainer);
        row.widget->setStyleSheet(
            "QWidget { background: #313244; border-radius: 4px; }");
        auto* rowLay = new QHBoxLayout(row.widget);
        rowLay->setContentsMargins(8, 6, 8, 6);
        rowLay->setSpacing(6);

        // Enabled toggle button (acts like checkbox)
        row.toggleBtn = new QPushButton(row.step.enabled ? "[ON]" : "[OFF]");
        row.toggleBtn->setFixedWidth(50);
        row.toggleBtn->setStyleSheet(row.step.enabled
            ? "QPushButton { background: #a6e3a1; color: #1e1e2e; border: none;"
              " border-radius: 3px; font-size: 10px; font-weight: bold; padding: 2px; }"
            : "QPushButton { background: #45475a; color: #a6adc8; border: none;"
              " border-radius: 3px; font-size: 10px; padding: 2px; }");
        int idx = i;
        connect(row.toggleBtn, &QPushButton::clicked, this, [this, idx]() {
            m_rows[idx].step.enabled = !m_rows[idx].step.enabled;
            rebuildStepList();
        });
        rowLay->addWidget(row.toggleBtn);

        // Step label
        row.label = new QLabel(row.step.label);
        row.label->setStyleSheet("color: #cdd6f4; font-size: 12px; background: transparent;");
        rowLay->addWidget(row.label, 1);

        // Up/Down buttons
        row.upBtn = new QPushButton("^");
        row.upBtn->setFixedSize(22, 22);
        row.upBtn->setEnabled(i > 0);
        connect(row.upBtn, &QPushButton::clicked, this, [this, idx]() {
            if (idx > 0) {
                m_rows.swapItemsAt(idx, idx - 1);
                rebuildStepList();
            }
        });
        rowLay->addWidget(row.upBtn);

        row.downBtn = new QPushButton("v");
        row.downBtn->setFixedSize(22, 22);
        row.downBtn->setEnabled(i < m_rows.size() - 1);
        connect(row.downBtn, &QPushButton::clicked, this, [this, idx]() {
            if (idx < m_rows.size() - 1) {
                m_rows.swapItemsAt(idx, idx + 1);
                rebuildStepList();
            }
        });
        rowLay->addWidget(row.downBtn);

        m_stepsLayout->addWidget(row.widget);
    }
    m_stepsLayout->addStretch();
}

void BuildChainDialog::onSave()
{
    BuildChainConfig cfg = currentConfig();
    cfg.save();
    emit chainSaved(cfg.name);
    accept();
}

void BuildChainDialog::onMoveUp()   { /* handled inline */ }
void BuildChainDialog::onMoveDown() { /* handled inline */ }
void BuildChainDialog::updateButtonStates() { /* handled inline */ }
