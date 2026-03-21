#include "core/custompipeline.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QSettings>
#include <QMessageBox>
#include <QInputDialog>
#include <QComboBox>

// ── Available checks ──────────────────────────────────────────────────────
QStringList PipelineConfig::availableChecks()
{
    return {"Security", "Code Quality", "Memory", "Dependencies", "Runtime"};
}

// ── Save / Load ───────────────────────────────────────────────────────────
void PipelineConfig::save() const
{
    if (name.isEmpty()) return;
    QSettings s("CodeClarity", "CodeClarity");
    s.beginGroup("pipelines/" + name);
    s.setValue("checks", checks);
    s.endGroup();

    // Register name in index
    QStringList names = s.value("pipelineNames").toStringList();
    if (!names.contains(name)) {
        names.append(name);
        s.setValue("pipelineNames", names);
    }
}

PipelineConfig PipelineConfig::load(const QString& name)
{
    PipelineConfig cfg;
    cfg.name = name;
    QSettings s("CodeClarity", "CodeClarity");
    s.beginGroup("pipelines/" + name);
    cfg.checks = s.value("checks").toStringList();
    s.endGroup();
    return cfg;
}

QStringList PipelineConfig::savedPipelineNames()
{
    QSettings s("CodeClarity", "CodeClarity");
    return s.value("pipelineNames").toStringList();
}

void PipelineConfig::remove(const QString& name)
{
    QSettings s("CodeClarity", "CodeClarity");
    s.remove("pipelines/" + name);
    QStringList names = s.value("pipelineNames").toStringList();
    names.removeAll(name);
    s.setValue("pipelineNames", names);
}

// ── PipelineRunner ────────────────────────────────────────────────────────
PipelineRunner::PipelineRunner(const PipelineConfig& cfg, QObject* parent)
    : QObject(parent), m_cfg(cfg)
{}

void PipelineRunner::run(const QString& code, const QString& lang, const QString& filePath)
{
    QList<PipelineResult> results;

    for (const QString& check : m_cfg.checks) {
        if (check == "Security") {
            results.append(runSecurityCheck(code, lang, filePath));
        } else if (check == "Code Quality") {
            results.append(runCodeQualityCheck(code, lang));
        } else if (check == "Memory") {
            results.append(runMemoryCheck(code, lang));
        } else if (check == "Dependencies") {
            results.append(runDependencyCheck(code, lang, filePath));
        } else if (check == "Runtime") {
            results.append(runRuntimeCheck(code, lang));
        }
    }

    emit finished(results);
}

PipelineResult PipelineRunner::runSecurityCheck(const QString& code, const QString& lang,
                                                 const QString& /*filePath*/) const
{
    PipelineResult r;
    r.checkName = "Security";

    // Simple pattern-based checks
    QStringList issues;
    if (lang == "python") {
        if (code.contains("eval("))        issues << "eval() usage detected";
        if (code.contains("exec("))        issues << "exec() usage detected";
        if (code.contains("pickle.load"))  issues << "Unsafe pickle deserialization";
        if (code.contains("os.system("))   issues << "Shell injection risk via os.system";
        if (code.contains("subprocess") && code.contains("shell=True"))
            issues << "shell=True subprocess risk";
    } else if (lang == "c" || lang == "cpp") {
        if (code.contains("gets("))        issues << "Unsafe gets() — use fgets()";
        if (code.contains("strcpy("))      issues << "Unsafe strcpy() — use strncpy()";
        if (code.contains("sprintf("))     issues << "Unsafe sprintf() — use snprintf()";
        if (code.contains("scanf(") && !code.contains("%s["))
            issues << "Unconstrained scanf %s — buffer overflow risk";
    }

    if (issues.isEmpty()) {
        r.passed  = true;
        r.summary = "No common security issues detected.";
    } else {
        r.passed  = false;
        r.summary = issues.join("; ");
    }
    return r;
}

PipelineResult PipelineRunner::runCodeQualityCheck(const QString& code, const QString& /*lang*/) const
{
    PipelineResult r;
    r.checkName = "Code Quality";

    QStringList issues;
    QStringList lines = code.split('\n');

    int longLines = 0;
    for (const QString& line : lines) {
        if (line.length() > 120) ++longLines;
    }
    if (longLines > 0)
        issues << QString("%1 lines exceed 120 characters").arg(longLines);

    // Check for TODO/FIXME/HACK comments
    int todoCount = code.count("TODO", Qt::CaseInsensitive)
                  + code.count("FIXME", Qt::CaseInsensitive)
                  + code.count("HACK", Qt::CaseInsensitive);
    if (todoCount > 0)
        issues << QString("%1 TODO/FIXME/HACK marker(s) in code").arg(todoCount);

    if (issues.isEmpty()) {
        r.passed  = true;
        r.summary = "Code quality looks good.";
    } else {
        r.passed  = false;
        r.summary = issues.join("; ");
    }
    return r;
}

PipelineResult PipelineRunner::runMemoryCheck(const QString& code, const QString& lang) const
{
    PipelineResult r;
    r.checkName = "Memory";

    if (lang != "c" && lang != "cpp") {
        r.passed  = true;
        r.summary = "Memory check only applies to C/C++ code.";
        return r;
    }

    QStringList issues;
    if (code.contains("malloc(") && !code.contains("free("))
        issues << "malloc() without free() — possible memory leak";
    if (code.contains("new ") && !code.contains("delete"))
        issues << "new without delete — possible memory leak";

    if (issues.isEmpty()) {
        r.passed  = true;
        r.summary = "No obvious memory management issues detected.";
    } else {
        r.passed  = false;
        r.summary = issues.join("; ");
    }
    return r;
}

PipelineResult PipelineRunner::runDependencyCheck(const QString& code, const QString& lang,
                                                    const QString& /*filePath*/) const
{
    PipelineResult r;
    r.checkName = "Dependencies";

    if (lang != "python") {
        r.passed  = true;
        r.summary = "Dependency check is currently Python-only.";
        return r;
    }

    // Count imports
    int importCount = 0;
    for (const QString& line : code.split('\n')) {
        QString trimmed = line.trimmed();
        if (trimmed.startsWith("import ") || trimmed.startsWith("from "))
            ++importCount;
    }

    r.passed  = true;
    r.summary = QString("%1 import(s) found. Run Dependency Analysis for full details.").arg(importCount);
    return r;
}

PipelineResult PipelineRunner::runRuntimeCheck(const QString& code, const QString& /*lang*/) const
{
    PipelineResult r;
    r.checkName = "Runtime";

    QStringList warnings;
    if (code.contains("while True") || code.contains("while(1)") || code.contains("while (1)"))
        warnings << "Infinite loop detected — ensure a break condition exists";

    int nestedDepth = 0;
    int maxNest = 0;
    for (QChar ch : code) {
        if (ch == '{' || ch == ':') ++nestedDepth;
        if (ch == '}') --nestedDepth;
        maxNest = qMax(maxNest, nestedDepth);
    }
    if (maxNest > 6)
        warnings << QString("Deep nesting detected (level %1) — may indicate complexity issues").arg(maxNest);

    if (warnings.isEmpty()) {
        r.passed  = true;
        r.summary = "No obvious runtime concerns detected.";
    } else {
        r.passed  = false;
        r.summary = warnings.join("; ");
    }
    return r;
}

// ── CustomPipelineDialog ─────────────────────────────────────────────────

static const char* STYLE_DLG =
    "QDialog { background: #1e1e2e; color: #cdd6f4; }"
    "QLabel { color: #cdd6f4; font-size: 12px; }"
    "QLineEdit { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
    "  border-radius: 4px; padding: 5px 8px; font-size: 12px; }"
    "QCheckBox { color: #cdd6f4; font-size: 12px; spacing: 8px; }"
    "QCheckBox::indicator { width: 14px; height: 14px; }"
    "QComboBox { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
    "  border-radius: 4px; padding: 4px 8px; font-size: 12px; }"
    "QComboBox QAbstractItemView { background: #313244; color: #cdd6f4; border: none; }"
    "QPushButton { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
    "  border-radius: 4px; padding: 6px 14px; font-size: 12px; }"
    "QPushButton:hover { background: #45475a; }"
    "QPushButton#primary { background: #89b4fa; color: #1e1e2e; border: none;"
    "  font-weight: 600; }"
    "QPushButton#primary:hover { background: #74a8e8; }";

CustomPipelineDialog::CustomPipelineDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Custom Test Pipeline");
    setModal(true);
    setMinimumSize(400, 360);
    setStyleSheet(STYLE_DLG);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(20, 16, 20, 16);
    lay->setSpacing(10);

    // Title
    auto* titleLbl = new QLabel("Custom Test Pipeline");
    titleLbl->setStyleSheet("font-size: 15px; font-weight: bold; color: #cdd6f4;");
    lay->addWidget(titleLbl);

    auto* descLbl = new QLabel(
        "Select the checks to include, give it a name, and save or run it.");
    descLbl->setStyleSheet("color: #a6adc8; font-size: 11px;");
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #313244;");
    lay->addWidget(sep);

    // Check boxes
    auto* checksBox = new QWidget;
    auto* checksLay = new QVBoxLayout(checksBox);
    checksLay->setSpacing(6);
    checksLay->setContentsMargins(0, 0, 0, 0);

    for (const QString& check : PipelineConfig::availableChecks()) {
        auto* cb = new QCheckBox(check);
        cb->setChecked(true);
        m_checkBoxes[check] = cb;
        checksLay->addWidget(cb);
    }
    lay->addWidget(checksBox);

    auto* sep2 = new QFrame;
    sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet("color: #313244;");
    lay->addWidget(sep2);

    // Name field
    auto* nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel("Pipeline name:"));
    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText("e.g. Full Security Audit");
    nameRow->addWidget(m_nameEdit, 1);
    lay->addLayout(nameRow);

    // Buttons
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    auto* loadBtn = new QPushButton("Load...");
    connect(loadBtn, &QPushButton::clicked, this, &CustomPipelineDialog::onLoad);
    btnRow->addWidget(loadBtn);

    btnRow->addStretch();

    auto* saveBtn = new QPushButton("Save Pipeline");
    connect(saveBtn, &QPushButton::clicked, this, &CustomPipelineDialog::onSave);
    btnRow->addWidget(saveBtn);

    auto* runBtn = new QPushButton("Run Now");
    runBtn->setObjectName("primary");
    connect(runBtn, &QPushButton::clicked, this, &CustomPipelineDialog::onRun);
    btnRow->addWidget(runBtn);

    auto* closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(closeBtn);

    lay->addLayout(btnRow);
}

PipelineConfig CustomPipelineDialog::currentConfig() const
{
    PipelineConfig cfg;
    cfg.name = m_nameEdit->text().trimmed();
    for (auto it = m_checkBoxes.begin(); it != m_checkBoxes.end(); ++it) {
        if (it.value()->isChecked())
            cfg.checks.append(it.key());
    }
    return cfg;
}

void CustomPipelineDialog::onSave()
{
    PipelineConfig cfg = currentConfig();
    if (cfg.name.isEmpty()) {
        QMessageBox::warning(this, "Name Required",
            "Please enter a name for this pipeline.");
        return;
    }
    cfg.save();
    QMessageBox::information(this, "Saved",
        QString("Pipeline \"%1\" saved.").arg(cfg.name));
    emit pipelineSaved();
}

void CustomPipelineDialog::onRun()
{
    PipelineConfig cfg = currentConfig();
    if (cfg.checks.isEmpty()) {
        QMessageBox::warning(this, "No Checks Selected",
            "Please select at least one check to run.");
        return;
    }
    emit runRequested(cfg);
    accept();
}

void CustomPipelineDialog::onLoad()
{
    QStringList names = PipelineConfig::savedPipelineNames();
    if (names.isEmpty()) {
        QMessageBox::information(this, "No Pipelines",
            "No saved pipelines found. Save one first.");
        return;
    }

    bool ok;
    QString name = QInputDialog::getItem(this, "Load Pipeline",
        "Choose a saved pipeline:", names, 0, false, &ok);
    if (!ok || name.isEmpty()) return;

    PipelineConfig cfg = PipelineConfig::load(name);
    m_nameEdit->setText(cfg.name);
    for (auto it = m_checkBoxes.begin(); it != m_checkBoxes.end(); ++it) {
        it.value()->setChecked(cfg.checks.contains(it.key()));
    }
}
