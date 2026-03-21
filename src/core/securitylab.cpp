#include "core/securitylab.h"
#include "core/jsonloader.h"
#include "core/theme.h"

using C = Theme::Colors;

#include <QJsonArray>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QMessageBox>
#include <QSizePolicy>
#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStackedWidget>
#include <QGroupBox>
#include <QTimer>
#include <QTextCursor>

// ── Shared style helpers ──────────────────────────────────────────────────────

static QString btnStyle(const QString& bg, const QString& hoverBg,
                        const QString& fg = QString())
{
    QString fgFinal = fg.isEmpty() ? QString(C::bg()) : fg;
    return QString(
        "QPushButton { background: %1; color: %3; border: none;"
        " border-radius: 4px; padding: 0 14px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: %2; }"
        "QPushButton:disabled { background: %4; color: %5; }")
        .arg(bg, hoverBg, fgFinal, C::bg2(), C::fg3());
}

// ── Lab program paths ─────────────────────────────────────────────────────────
// Labs live next to the executable in a "labs/" subfolder at runtime.

static QString labsDir()
{
    // Prefer next to executable (deployed build)
    QString exeDir = QApplication::applicationDirPath();
    QString candidate = exeDir + "/labs";
    if (QDir(candidate).exists()) return candidate;

    // Fall back to source tree (development)
    // Walk up from exe until we find src/labs
    QDir d(exeDir);
    for (int i = 0; i < 6; ++i) {
        QString src = d.absolutePath() + "/src/labs";
        if (QDir(src).exists()) return src;
        if (!d.cdUp()) break;
    }
    return candidate; // best guess even if not found
}

// ── Lab definitions — loaded from data/security_labs.json ────────────────────

QList<LabDefinition> SecurityLabWidget::allLabs()
{
    QString dir = labsDir();
    QJsonArray arr = JsonLoader::loadArray("security_labs.json", "labs");
    QList<LabDefinition> labs;
    labs.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        QJsonObject o = v.toObject();
        LabDefinition d;
        d.name           = o["name"].toString();
        d.vulnType       = o["vulnType"].toString();
        d.isPython       = o["isPython"].toBool();
        d.labProgram     = dir + "/" + o["labProgram"].toString();
        d.labProgram2    = dir + "/" + o["labProgram2"].toString();
        d.normalInput    = o["normalInput"].toString();
        d.attackInput    = o["attackInput"].toString();
        d.vulnCode       = o["vulnCode"].toString();
        d.fixedCode      = o["fixedCode"].toString();
        d.explanation    = o["explanation"].toString();
        d.fixExplanation = o["fixExplanation"].toString();
        labs.append(d);
    }
    return labs;
}

SecurityLabWidget* SecurityLabWidget::forVulnType(const QString& vulnType, QWidget* parent)
{
    for (const auto& lab : allLabs()) {
        if (lab.vulnType == vulnType)
            return new SecurityLabWidget(lab, parent);
    }
    return nullptr;
}

// ── Constructor ───────────────────────────────────────────────────────────────

SecurityLabWidget::SecurityLabWidget(const LabDefinition& lab, QWidget* parent)
    : AnalysisFrame(QString("Security Labs — %1").arg(lab.name), parent), m_lab(lab)
{
    // ── Action buttons ───────────────────────────────────────────────────────
    auto* btnBar = new QHBoxLayout;
    btnBar->setSpacing(8);

    m_runNormalBtn = new QPushButton("Run Normal Input");
    m_runNormalBtn->setFixedHeight(30);
    m_runNormalBtn->setCursor(Qt::PointingHandCursor);
    m_runNormalBtn->setStyleSheet(btnStyle(C::green(), C::green()));
    connect(m_runNormalBtn, &QPushButton::clicked, this, &SecurityLabWidget::runNormal);
    btnBar->addWidget(m_runNormalBtn);

    m_runAttackBtn = new QPushButton("Run Attack Input");
    m_runAttackBtn->setFixedHeight(30);
    m_runAttackBtn->setCursor(Qt::PointingHandCursor);
    m_runAttackBtn->setStyleSheet(btnStyle(C::red(), C::red()));
    connect(m_runAttackBtn, &QPushButton::clicked, this, &SecurityLabWidget::runAttack);
    btnBar->addWidget(m_runAttackBtn);

    m_fixBtn = new QPushButton("Show the Fix");
    m_fixBtn->setFixedHeight(30);
    m_fixBtn->setCursor(Qt::PointingHandCursor);
    m_fixBtn->setStyleSheet(btnStyle(C::accent(), C::accent()));
    connect(m_fixBtn, &QPushButton::clicked, this, &SecurityLabWidget::showFix);
    btnBar->addWidget(m_fixBtn);

    m_whyBtn = new QPushButton("Why This Matters");
    m_whyBtn->setFixedHeight(30);
    m_whyBtn->setCursor(Qt::PointingHandCursor);
    m_whyBtn->setStyleSheet(btnStyle(C::yellow(), C::yellow()));
    connect(m_whyBtn, &QPushButton::clicked, this, &SecurityLabWidget::showWhyItMatters);
    btnBar->addWidget(m_whyBtn);

    btnBar->addStretch(1);
    auto* btnBarWidget = new QWidget;
    btnBarWidget->setLayout(btnBar);

    // ── 3-pane splitter ──────────────────────────────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(2);
    splitter->setStyleSheet(QString("QSplitter::handle { background: %1; }").arg(C::bg2()));

    // ── LEFT: Code pane ──────────────────────────────────────────────────────
    auto* codeContainer = new QWidget;
    codeContainer->setStyleSheet(QString("background: %1;").arg(C::bg()));
    auto* codeLayout = new QVBoxLayout(codeContainer);
    codeLayout->setContentsMargins(0, 0, 0, 0);
    codeLayout->setSpacing(0);

    auto* codeHeader = new QLabel("  Code");
    codeHeader->setFixedHeight(24);
    codeHeader->setStyleSheet(
        QString("background: %1; color: %2; font-size: 11px; font-weight: bold;"
        " border-bottom: 1px solid %3; padding-left: 6px;")
        .arg(C::bg2(), C::fg2(), C::border()));
    codeLayout->addWidget(codeHeader);

    m_codeStack = new QStackedWidget;

    QFont monoFont("Consolas", 11);
    monoFont.setFixedPitch(true);

    m_codePaneVuln = new QPlainTextEdit;
    m_codePaneVuln->setReadOnly(true);
    m_codePaneVuln->setFont(monoFont);
    m_codePaneVuln->setPlainText(lab.vulnCode);
    m_codePaneVuln->setStyleSheet(
        QString("QPlainTextEdit { background: %1; color: %2; border: none; padding: 6px; }"
        "QScrollBar:vertical { background: %1; width: 8px; }"
        "QScrollBar::handle:vertical { background: %3; border-radius: 3px; }")
        .arg(C::bg(), C::fg(), C::border()));
    m_codeStack->addWidget(m_codePaneVuln);

    m_codePaneFix = new QPlainTextEdit;
    m_codePaneFix->setReadOnly(true);
    m_codePaneFix->setFont(monoFont);
    m_codePaneFix->setPlainText(lab.fixedCode);
    m_codePaneFix->setStyleSheet(
        QString("QPlainTextEdit { background: %1; color: %2; border: none; padding: 6px; }"
        "QScrollBar:vertical { background: %1; width: 8px; }"
        "QScrollBar::handle:vertical { background: %3; border-radius: 3px; }")
        .arg(C::bg(), C::green(), C::border()));
    m_codeStack->addWidget(m_codePaneFix);

    m_codeStack->setCurrentIndex(0);
    codeLayout->addWidget(m_codeStack, 1);

    splitter->addWidget(codeContainer);

    // ── CENTER: Input pane ───────────────────────────────────────────────────
    auto* inputContainer = new QWidget;
    inputContainer->setStyleSheet(QString("background: %1;").arg(C::bg()));
    auto* inputLayout = new QVBoxLayout(inputContainer);
    inputLayout->setContentsMargins(8, 8, 8, 8);
    inputLayout->setSpacing(8);

    auto* inputHeader = new QLabel("Input");
    inputHeader->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: bold; padding: 0;").arg(C::fg2()));
    inputLayout->addWidget(inputHeader);

    // SQL and XSS get a mock webpage UI; others get a plain text field
    bool isSql = (lab.vulnType == "sql_injection");
    bool isXss = (lab.vulnType == "xss");

    if (isSql) {
        buildSqlMockUi(inputContainer, inputLayout);
    } else if (isXss) {
        buildXssMockUi(inputContainer, inputLayout);
    } else {
        // Generic input field
        auto* inputLabel = new QLabel("Program input:");
        inputLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(C::fg2()));
        inputLayout->addWidget(inputLabel);

        m_inputField = new QLineEdit;
        m_inputField->setText(lab.normalInput);
        m_inputField->setFont(monoFont);
        m_inputField->setStyleSheet(
            QString("QLineEdit { background: %1; color: %2; border: 1px solid %3;"
            " border-radius: 4px; padding: 6px; font-size: 13px; }")
            .arg(C::bg(), C::fg(), C::border()));
        inputLayout->addWidget(m_inputField);

        auto* hint = new QLabel(
            QString("Normal: %1\nAttack: %2")
            .arg(lab.normalInput, lab.attackInput));
        hint->setStyleSheet(QString("color: %1; font-size: 11px;").arg(C::fg3()));
        hint->setWordWrap(true);
        inputLayout->addWidget(hint);
    }

    inputLayout->addStretch(1);
    splitter->addWidget(inputContainer);

    // ── RIGHT: Output pane ───────────────────────────────────────────────────
    auto* outputContainer = new QWidget;
    outputContainer->setStyleSheet(QString("background: %1;").arg(C::bg()));
    auto* outputLayout = new QVBoxLayout(outputContainer);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setSpacing(0);

    auto* outputHeader = new QLabel("  Output / Shell");
    outputHeader->setFixedHeight(24);
    outputHeader->setStyleSheet(
        QString("background: %1; color: %2; font-size: 11px; font-weight: bold;"
        " border-bottom: 1px solid %3; padding-left: 6px;")
        .arg(C::bg2(), C::fg2(), C::border()));
    outputLayout->addWidget(outputHeader);

    m_outputPane = new QTextEdit;
    m_outputPane->setReadOnly(true);
    m_outputPane->setFont(monoFont);
    m_outputPane->setStyleSheet(
        QString("QTextEdit { background: %1; color: %2; border: none; padding: 6px; }"
        "QScrollBar:vertical { background: %1; width: 8px; }"
        "QScrollBar::handle:vertical { background: %3; border-radius: 3px; }")
        .arg(C::bg(), C::fg(), C::border()));
    outputLayout->addWidget(m_outputPane, 1);

    splitter->addWidget(outputContainer);

    splitter->setSizes({380, 260, 360});

    // Add controls into the inherited results area
    addResultWidget(btnBarWidget);
    addResultWidget(splitter);

    // ── Initial status ────────────────────────────────────────────────────────
    setStatus("Press a Run button to see what happens.");
}

// ── SQL mock UI ───────────────────────────────────────────────────────────────

void SecurityLabWidget::buildSqlMockUi(QWidget* /*parent*/, QVBoxLayout* layout)
{
    // Styled box that looks like a login form
    auto* frame = new QFrame;
    frame->setStyleSheet(
        QString("QFrame { background: %1; border: 1px solid %2; border-radius: 6px; }")
        .arg(C::bg2(), C::border()));
    auto* fl = new QVBoxLayout(frame);
    fl->setContentsMargins(14, 12, 14, 12);
    fl->setSpacing(10);

    auto* siteTitle = new QLabel("Login");
    siteTitle->setStyleSheet(
        QString("color: %1; font-size: 15px; font-weight: bold; border: none;").arg(C::fg()));
    fl->addWidget(siteTitle);

    auto* unLabel = new QLabel("Username");
    unLabel->setStyleSheet(QString("color: %1; font-size: 12px; border: none;").arg(C::fg2()));
    fl->addWidget(unLabel);

    m_sqlUsername = new QLineEdit;
    m_sqlUsername->setText(m_lab.normalInput.split('\n').value(0));
    m_sqlUsername->setStyleSheet(
        QString("QLineEdit { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 4px; padding: 6px; font-size: 13px; }")
        .arg(C::bg(), C::fg(), C::border()));
    fl->addWidget(m_sqlUsername);

    auto* pwLabel = new QLabel("Password");
    pwLabel->setStyleSheet(QString("color: %1; font-size: 12px; border: none;").arg(C::fg2()));
    fl->addWidget(pwLabel);

    m_sqlPassword = new QLineEdit;
    m_sqlPassword->setEchoMode(QLineEdit::Password);
    m_sqlPassword->setText(m_lab.normalInput.split('\n').value(1, "password123"));
    m_sqlPassword->setStyleSheet(
        QString("QLineEdit { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 4px; padding: 6px; font-size: 13px; }")
        .arg(C::bg(), C::fg(), C::border()));
    fl->addWidget(m_sqlPassword);

    auto* loginBtn = new QPushButton("Login");
    loginBtn->setFixedHeight(32);
    loginBtn->setCursor(Qt::PointingHandCursor);
    loginBtn->setStyleSheet(btnStyle(C::accent(), C::accent()));
    connect(loginBtn, &QPushButton::clicked, this, [this]() {
        QString input = m_sqlUsername->text() + "\n" + m_sqlPassword->text();
        runWithInput(input);
    });
    fl->addWidget(loginBtn);

    layout->addWidget(frame);

    auto* hint = new QLabel(
        "Try injecting:  ' OR '1'='1\ninto the Username field, then press Login");
    hint->setStyleSheet(QString("color: %1; font-size: 11px;").arg(C::fg3()));
    hint->setWordWrap(true);
    layout->addWidget(hint);
}

// ── XSS mock UI ──────────────────────────────────────────────────────────────

void SecurityLabWidget::buildXssMockUi(QWidget* /*parent*/, QVBoxLayout* layout)
{
    auto* frame = new QFrame;
    frame->setStyleSheet(
        QString("QFrame { background: %1; border: 1px solid %2; border-radius: 6px; }")
        .arg(C::bg2(), C::border()));
    auto* fl = new QVBoxLayout(frame);
    fl->setContentsMargins(14, 12, 14, 12);
    fl->setSpacing(10);

    auto* siteTitle = new QLabel("Post a Comment");
    siteTitle->setStyleSheet(
        QString("color: %1; font-size: 15px; font-weight: bold; border: none;").arg(C::fg()));
    fl->addWidget(siteTitle);

    auto* cmtLabel = new QLabel("Your comment:");
    cmtLabel->setStyleSheet(QString("color: %1; font-size: 12px; border: none;").arg(C::fg2()));
    fl->addWidget(cmtLabel);

    m_xssComment = new QTextEdit;
    m_xssComment->setFixedHeight(80);
    m_xssComment->setText(m_lab.normalInput);
    m_xssComment->setStyleSheet(
        QString("QTextEdit { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 4px; padding: 6px; font-size: 13px; }")
        .arg(C::bg(), C::fg(), C::border()));
    fl->addWidget(m_xssComment);

    auto* postBtn = new QPushButton("Post Comment");
    postBtn->setFixedHeight(32);
    postBtn->setCursor(Qt::PointingHandCursor);
    postBtn->setStyleSheet(btnStyle(C::accent(), C::accent()));
    connect(postBtn, &QPushButton::clicked, this, [this]() {
        runWithInput(m_xssComment->toPlainText());
    });
    fl->addWidget(postBtn);

    layout->addWidget(frame);

    auto* hint = new QLabel(
        "Try injecting:  <script>document.cookie</script>\ninto the comment box");
    hint->setStyleSheet(QString("color: %1; font-size: 11px;").arg(C::fg3()));
    hint->setWordWrap(true);
    layout->addWidget(hint);
}

// ── Run buttons ───────────────────────────────────────────────────────────────

void SecurityLabWidget::runNormal()
{
    // Update mock UI fields if present
    if (m_sqlUsername) {
        QStringList parts = m_lab.normalInput.split('\n');
        m_sqlUsername->setText(parts.value(0));
        m_sqlPassword->setText(parts.value(1, "password123"));
        runWithInput(m_lab.normalInput);
        return;
    }
    if (m_xssComment) {
        m_xssComment->setText(m_lab.normalInput);
        runWithInput(m_lab.normalInput);
        return;
    }
    QString input = m_inputField ? m_inputField->text() : m_lab.normalInput;
    runWithInput(input.isEmpty() ? m_lab.normalInput : input);
}

void SecurityLabWidget::runAttack()
{
    if (m_sqlUsername) {
        QStringList parts = m_lab.attackInput.split('\n');
        m_sqlUsername->setText(parts.value(0));
        m_sqlPassword->setText(parts.value(1, "anything"));
        runWithInput(m_lab.attackInput);
        return;
    }
    if (m_xssComment) {
        m_xssComment->setText(m_lab.attackInput);
        runWithInput(m_lab.attackInput);
        return;
    }
    if (m_inputField) m_inputField->setText(m_lab.attackInput);
    runWithInput(m_lab.attackInput);
}

// ── Core execution ────────────────────────────────────────────────────────────

void SecurityLabWidget::runWithInput(const QString& input)
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }

    m_outputPane->clear();
    m_pendingInput = input;

    bool showingFix = (m_codeStack->currentIndex() == 1);
    QString program = showingFix ? m_lab.labProgram2 : m_lab.labProgram;

    if (m_lab.isPython) {
        // Run with python
        if (!m_process) {
            m_process = new QProcess(this);
            connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
                appendOutput(QString::fromLocal8Bit(m_process->readAllStandardOutput()));
            });
            connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
                appendOutput(QString::fromLocal8Bit(m_process->readAllStandardError()), C::red());
            });
            connect(m_process,
                    QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, &SecurityLabWidget::onProcessFinished);
            connect(m_process, &QProcess::errorOccurred,
                    this, &SecurityLabWidget::onProcessError);
        }

        // Find python executable
        QString pythonExe;
        for (const QString& candidate : {"python3", "python", "python3.exe", "python.exe"}) {
            if (QProcess::execute(candidate, {"--version"}) == 0) {
                pythonExe = candidate;
                break;
            }
        }
        if (pythonExe.isEmpty()) pythonExe = "python";

        m_statusLabel->setText("Running...");
        m_runNormalBtn->setEnabled(false);
        m_runAttackBtn->setEnabled(false);

        m_process->start(pythonExe, {program});
        m_process->write((input + "\n").toLocal8Bit());
        m_process->closeWriteChannel();

    } else {
        // C program — on Windows run the .exe; compile first if not present
        QString exePath = program;
#ifdef Q_OS_WIN
        if (!exePath.endsWith(".exe")) exePath += ".exe";
#endif
        if (!QFileInfo::exists(exePath)) {
            // Try to compile with gcc
            QString srcPath = program + ".c";
#ifdef Q_OS_WIN
            srcPath = program.chopped(0) + ".c"; // program without .exe
            if (program.endsWith(".exe"))
                srcPath = program.left(program.length()-4) + ".c";
            else
                srcPath = program + ".c";
#endif
            appendOutput("Compiling " + QFileInfo(srcPath).fileName() + "...\n", C::yellow());
            QProcess compiler;
            compiler.start("gcc", {srcPath, "-o", exePath, "-w"});
            compiler.waitForFinished(10000);
            if (compiler.exitCode() != 0) {
                appendOutput("Compilation failed:\n", C::red());
                appendOutput(QString::fromLocal8Bit(compiler.readAllStandardError()), C::red());
                m_statusLabel->setText("Compilation failed.");
                return;
            }
            appendOutput("Compiled OK.\n\n", C::green());
        }

        if (!m_process) {
            m_process = new QProcess(this);
            connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
                appendOutput(QString::fromLocal8Bit(m_process->readAllStandardOutput()));
            });
            connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
                appendOutput(QString::fromLocal8Bit(m_process->readAllStandardError()), C::red());
            });
            connect(m_process,
                    QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, &SecurityLabWidget::onProcessFinished);
            connect(m_process, &QProcess::errorOccurred,
                    this, &SecurityLabWidget::onProcessError);
        }

        m_statusLabel->setText("Running...");
        m_runNormalBtn->setEnabled(false);
        m_runAttackBtn->setEnabled(false);

        m_process->start(exePath, {});
        m_process->write((input + "\n").toLocal8Bit());
        m_process->closeWriteChannel();
    }

    // Safety timeout — kill after 10 seconds
    QTimer::singleShot(10000, this, [this]() {
        if (m_process && m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            appendOutput("\n[Process killed — exceeded 10 second timeout]\n", C::red());
        }
    });
}

void SecurityLabWidget::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    m_runNormalBtn->setEnabled(true);
    m_runAttackBtn->setEnabled(true);

    if (status == QProcess::CrashExit) {
        appendOutput("\n[Process crashed or was killed by OS]\n", C::red());
        m_statusLabel->setText("Process crashed.");
    } else if (exitCode != 0) {
        appendOutput(QString("\n[Process exited with code %1]\n").arg(exitCode), C::yellow());
        m_statusLabel->setText(QString("Exited: code %1").arg(exitCode));
    } else {
        m_statusLabel->setText("Done.");
    }
}

void SecurityLabWidget::onProcessError(QProcess::ProcessError err)
{
    m_runNormalBtn->setEnabled(true);
    m_runAttackBtn->setEnabled(true);

    QString msg;
    switch (err) {
    case QProcess::FailedToStart:
        msg = "Failed to start program. Is Python installed?";
        break;
    case QProcess::Crashed:
        msg = "Process crashed.";
        break;
    default:
        msg = "Process error.";
        break;
    }
    appendOutput("\n[ERROR] " + msg + "\n", C::red());
    m_statusLabel->setText(msg);
}

// ── Show Fix ──────────────────────────────────────────────────────────────────

void SecurityLabWidget::showFix()
{
    m_showingFix = !m_showingFix;
    m_codeStack->setCurrentIndex(m_showingFix ? 1 : 0);

    if (m_showingFix) {
        m_fixBtn->setText("Show Vulnerable Code");
        m_fixBtn->setStyleSheet(btnStyle(C::green(), C::green()));
        appendOutput("\n--- FIXED VERSION ---\n", C::green());
        appendOutput(m_lab.fixExplanation + "\n", C::green());
    } else {
        m_fixBtn->setText("Show the Fix");
        m_fixBtn->setStyleSheet(btnStyle(C::accent(), C::accent()));
        appendOutput("\n--- VULNERABLE VERSION ---\n", C::red());
    }
}

// ── Why This Matters ──────────────────────────────────────────────────────────

void SecurityLabWidget::showWhyItMatters()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Why This Matters");
    dlg->setModal(true);
    dlg->setMinimumWidth(460);
    dlg->setStyleSheet(QString("background: %1;").arg(C::bg()));

    auto* lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(22, 18, 22, 18);
    lay->setSpacing(14);

    auto* title = new QLabel(m_lab.name);
    title->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: bold;").arg(C::red()));
    lay->addWidget(title);

    auto* body = new QLabel(m_lab.explanation);
    body->setWordWrap(true);
    body->setStyleSheet(QString("color: %1; font-size: 13px; line-height: 150%;").arg(C::fg()));
    lay->addWidget(body);

    auto* closeBtn = new QPushButton("Close");
    closeBtn->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 6px; padding: 0 18px; font-size: 13px; min-height: 30px; }"
        "QPushButton:hover { background: %3; }")
        .arg(C::bg2(), C::fg(), C::border()));
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    lay->addLayout(btnRow);

    dlg->exec();
    dlg->deleteLater();
}

// ── Output helper ─────────────────────────────────────────────────────────────

void SecurityLabWidget::appendOutput(const QString& text, const QString& color)
{
    if (color.isEmpty() || color == C::fg()) {
        m_outputPane->moveCursor(QTextCursor::End);
        m_outputPane->insertPlainText(text);
    } else {
        QString html = QString("<span style=\"color:%1;\">%2</span>")
            .arg(color, text.toHtmlEscaped().replace("\n", "<br>"));
        m_outputPane->moveCursor(QTextCursor::End);
        m_outputPane->insertHtml(html);
    }
    m_outputPane->moveCursor(QTextCursor::End);
}
