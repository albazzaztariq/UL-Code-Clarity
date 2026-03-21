#include "core/whatif.h"
#include "core/theme.h"

#include <QRegularExpression>
#include <QSplitter>
#include <QGridLayout>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QUuid>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QTextBlock>
#include <QTextCursor>

// ─── helpers ─────────────────────────────────────────────────────────────────

static QString runnerFor(const QString& language)
{
    if (language == "python" || language == "py") return "python";
    if (language == "javascript" || language == "js") return "node";
    return QString(); // compiled languages not supported in sandbox
}

static QString detectToken(const QString& lineText)
{
    // Try to find a numeric literal or a simple identifier = value assignment.
    // Returns the first token after '=' if present, otherwise the first
    // standalone number, else empty string.
    static QRegularExpression assignRe(R"(=\s*([^\s,\)\]\}#]+))");
    auto m = assignRe.match(lineText);
    if (m.hasMatch()) return m.captured(1).trimmed();

    static QRegularExpression numRe(R"(\b(\d+(?:\.\d+)?)\b)");
    auto m2 = numRe.match(lineText);
    if (m2.hasMatch()) return m2.captured(1);

    return QString();
}

// ─── WhatIfDialog ─────────────────────────────────────────────────────────────

WhatIfDialog::WhatIfDialog(const QString& filePath,
                           int lineNumber,
                           const QString& lineText,
                           const QString& language,
                           QWidget* parent)
    : QDialog(parent)
    , m_filePath(filePath)
    , m_lineNumber(lineNumber)
    , m_lineText(lineText)
    , m_language(language)
{
    setWindowTitle(QString("What If? — Line %1").arg(lineNumber));
    setMinimumSize(700, 480);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    // ── Title ────────────────────────────────────────────────────────────────
    auto* titleLbl = new QLabel("What If Sandbox");
    titleLbl->setStyleSheet(
        "color: #89b4fa; font-size: 14px; font-weight: bold;");
    root->addWidget(titleLbl);

    // ── Line text display ────────────────────────────────────────────────────
    m_lineLabel = new QLabel(
        QString("Line %1:  <code>%2</code>")
        .arg(lineNumber)
        .arg(lineText.trimmed().toHtmlEscaped()));
    m_lineLabel->setTextFormat(Qt::RichText);
    m_lineLabel->setStyleSheet(
        "color: #a6adc8; font-size: 11px;"
        "background: #2a2a3c; padding: 4px 8px; border-radius: 4px;");
    m_lineLabel->setWordWrap(true);
    root->addWidget(m_lineLabel);

    // ── Controls row ─────────────────────────────────────────────────────────
    auto* ctrlRow = new QHBoxLayout;
    ctrlRow->setSpacing(8);

    auto* changeLbl = new QLabel("Change");
    changeLbl->setStyleSheet("color: #cdd6f4; font-size: 12px;");
    ctrlRow->addWidget(changeLbl);

    m_oldValueEdit = new QLineEdit;
    m_oldValueEdit->setPlaceholderText("old value");
    m_oldValueEdit->setFixedWidth(130);
    QString detected = detectToken(lineText);
    m_oldValueEdit->setText(detected);
    ctrlRow->addWidget(m_oldValueEdit);

    auto* toLbl = new QLabel("to");
    toLbl->setStyleSheet("color: #cdd6f4; font-size: 12px;");
    ctrlRow->addWidget(toLbl);

    m_newValueEdit = new QLineEdit;
    m_newValueEdit->setPlaceholderText("new value");
    m_newValueEdit->setFixedWidth(130);
    ctrlRow->addWidget(m_newValueEdit);

    ctrlRow->addStretch();

    m_runOrigBtn = new QPushButton("Run Original");
    m_runOrigBtn->setCursor(Qt::PointingHandCursor);
    m_runOrigBtn->setStyleSheet(
        "QPushButton { background: #45475a; color: #cdd6f4; border-radius: 5px;"
        " padding: 5px 14px; font-size: 12px; }"
        "QPushButton:hover { background: #585b70; }");
    ctrlRow->addWidget(m_runOrigBtn);

    m_runModBtn = new QPushButton("Run Modified");
    m_runModBtn->setCursor(Qt::PointingHandCursor);
    m_runModBtn->setStyleSheet(
        "QPushButton { background: #45475a; color: #cdd6f4; border-radius: 5px;"
        " padding: 5px 14px; font-size: 12px; }"
        "QPushButton:hover { background: #585b70; }");
    ctrlRow->addWidget(m_runModBtn);

    m_runBothBtn = new QPushButton("Run Both");
    m_runBothBtn->setCursor(Qt::PointingHandCursor);
    m_runBothBtn->setStyleSheet(
        "QPushButton { background: #89b4fa; color: #1e1e2e; border-radius: 5px;"
        " padding: 5px 14px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background: #b4d0ff; }");
    ctrlRow->addWidget(m_runBothBtn);

    root->addLayout(ctrlRow);

    // ── Side-by-side output panes ─────────────────────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);

    // Left — original
    auto* leftContainer = new QWidget;
    auto* leftLay = new QVBoxLayout(leftContainer);
    leftLay->setContentsMargins(0,0,0,0);
    leftLay->setSpacing(4);
    m_leftLabel = new QLabel("Original Output");
    m_leftLabel->setStyleSheet("color: #a6e3a1; font-size: 11px; font-weight: bold; padding: 2px 4px;");
    leftLay->addWidget(m_leftLabel);
    m_leftPane = new QPlainTextEdit;
    m_leftPane->setReadOnly(true);
    m_leftPane->setPlaceholderText("Run the original file to see output here...");
    m_leftPane->setStyleSheet(
        "QPlainTextEdit { background: #181825; color: #cdd6f4; border: 1px solid #313244;"
        " font-family: 'Cascadia Code','Consolas',monospace; font-size: 12px; }");
    leftLay->addWidget(m_leftPane);
    splitter->addWidget(leftContainer);

    // Right — modified
    auto* rightContainer = new QWidget;
    auto* rightLay = new QVBoxLayout(rightContainer);
    rightLay->setContentsMargins(0,0,0,0);
    rightLay->setSpacing(4);
    m_rightLabel = new QLabel("Modified Output");
    m_rightLabel->setStyleSheet("color: #f38ba8; font-size: 11px; font-weight: bold; padding: 2px 4px;");
    rightLay->addWidget(m_rightLabel);
    m_rightPane = new QPlainTextEdit;
    m_rightPane->setReadOnly(true);
    m_rightPane->setPlaceholderText("Run the modified file to see output here...");
    m_rightPane->setStyleSheet(
        "QPlainTextEdit { background: #181825; color: #cdd6f4; border: 1px solid #313244;"
        " font-family: 'Cascadia Code','Consolas',monospace; font-size: 12px; }");
    rightLay->addWidget(m_rightPane);
    splitter->addWidget(rightContainer);

    splitter->setSizes({340, 340});
    root->addWidget(splitter, 1);

    // ── Note for unsupported languages ────────────────────────────────────────
    if (runnerFor(language).isEmpty()) {
        auto* noteLbl = new QLabel(
            QString("Note: Live execution sandbox supports Python and JavaScript. "
                    "For %1, you can still edit the values above.").arg(language));
        noteLbl->setWordWrap(true);
        noteLbl->setStyleSheet("color: #f9e2af; font-size: 11px;");
        root->addWidget(noteLbl);
        m_runBothBtn->setEnabled(false);
        m_runOrigBtn->setEnabled(false);
        m_runModBtn->setEnabled(false);
    }

    connect(m_runBothBtn, &QPushButton::clicked, this, &WhatIfDialog::onRunBothClicked);
    connect(m_runOrigBtn, &QPushButton::clicked, this, &WhatIfDialog::onRunOriginalClicked);
    connect(m_runModBtn,  &QPushButton::clicked, this, &WhatIfDialog::onRunModifiedClicked);
}

WhatIfDialog::~WhatIfDialog()
{
    if (m_origProcess) { m_origProcess->kill(); m_origProcess->deleteLater(); }
    if (m_modProcess)  { m_modProcess->kill();  m_modProcess->deleteLater(); }
}

// ─── Button handlers ──────────────────────────────────────────────────────────

void WhatIfDialog::onRunBothClicked()
{
    m_origOutput.clear();
    m_modOutput.clear();
    m_leftPane->clear();
    m_rightPane->clear();
    m_pendingRuns = 2;
    runFile(m_filePath, false);
    QString tmp = buildModifiedTempFile();
    if (!tmp.isEmpty())
        runFile(tmp, true);
    else {
        m_pendingRuns = 1;
        m_rightPane->setPlainText("[Error: could not create modified temp file]");
    }
}

void WhatIfDialog::onRunOriginalClicked()
{
    m_origOutput.clear();
    m_leftPane->clear();
    m_pendingRuns = 1;
    runFile(m_filePath, false);
}

void WhatIfDialog::onRunModifiedClicked()
{
    m_modOutput.clear();
    m_rightPane->clear();
    QString tmp = buildModifiedTempFile();
    if (tmp.isEmpty()) {
        m_rightPane->setPlainText("[Error: could not create modified temp file]");
        return;
    }
    m_pendingRuns = 1;
    runFile(tmp, true);
}

// ─── Execution ────────────────────────────────────────────────────────────────

void WhatIfDialog::runFile(const QString& filePath, bool isModified)
{
    QString runner = runnerFor(m_language);
    if (runner.isEmpty()) return;

    auto* proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::MergedChannels);

    if (isModified) {
        if (m_modProcess) { m_modProcess->kill(); m_modProcess->deleteLater(); }
        m_modProcess  = proc;
        m_rightPane->setPlainText("Running...");
    } else {
        if (m_origProcess) { m_origProcess->kill(); m_origProcess->deleteLater(); }
        m_origProcess = proc;
        m_leftPane->setPlainText("Running...");
    }

    // Capture the filePath value by value in lambda
    QString capturedFile = filePath;
    bool    capturedMod  = isModified;

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, [this, proc, capturedFile, capturedMod](int, QProcess::ExitStatus) {
            QString out = QString::fromUtf8(proc->readAll());
            proc->deleteLater();
            if (capturedMod) {
                m_modProcess = nullptr;
                // Clean up temp file
                QFile::remove(capturedFile);
            } else {
                m_origProcess = nullptr;
            }

            if (capturedMod)
                finishModified(out);
            else
                finishOriginal(out);
        });

    proc->start(runner, {filePath});
}

void WhatIfDialog::finishOriginal(const QString& output)
{
    m_origOutput = output;
    m_leftPane->setPlainText(output);

    --m_pendingRuns;
    if (m_pendingRuns == 0 && !m_origOutput.isEmpty() && !m_modOutput.isEmpty())
        showDiff(m_origOutput, m_modOutput);
}

void WhatIfDialog::finishModified(const QString& output)
{
    m_modOutput = output;
    m_rightPane->setPlainText(output);

    --m_pendingRuns;
    if (m_pendingRuns == 0 && !m_origOutput.isEmpty() && !m_modOutput.isEmpty())
        showDiff(m_origOutput, m_modOutput);
}

// ─── Temp file builder ────────────────────────────────────────────────────────

QString WhatIfDialog::buildModifiedTempFile() const
{
    QFile src(m_filePath);
    if (!src.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();

    QStringList lines;
    {
        QTextStream ts(&src);
        while (!ts.atEnd())
            lines << ts.readLine();
    }

    if (m_lineNumber < 1 || m_lineNumber > lines.size()) return QString();

    QString oldVal = m_oldValueEdit->text();
    QString newVal = m_newValueEdit->text();
    if (oldVal.isEmpty()) return QString();

    // Substitute on target line only
    lines[m_lineNumber - 1].replace(oldVal, newVal);

    QFileInfo fi(m_filePath);
    QString tmpPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                    + "/cc_whatif_"
                    + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8)
                    + "." + fi.suffix();

    QFile out(tmpPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) return QString();
    QTextStream ts(&out);
    for (const QString& line : lines)
        ts << line << "\n";

    return tmpPath;
}

// ─── Diff highlighting ────────────────────────────────────────────────────────

void WhatIfDialog::showDiff(const QString& original, const QString& modified)
{
    QStringList origLines = original.split('\n');
    QStringList modLines  = modified.split('\n');
    int maxLen = qMax(origLines.size(), modLines.size());

    // Build highlighted documents
    m_leftPane->clear();
    m_rightPane->clear();

    QTextCharFormat diffFmtOrig;
    diffFmtOrig.setBackground(QColor(0x8b, 0x28, 0x28, 180));  // dark red tint

    QTextCharFormat diffFmtMod;
    diffFmtMod.setBackground(QColor(0x28, 0x6b, 0x38, 180));   // dark green tint

    QTextCharFormat normalFmt;

    auto appendLine = [](QPlainTextEdit* pane,
                         const QString& text,
                         const QTextCharFormat& fmt) {
        QTextCursor cur = pane->textCursor();
        cur.movePosition(QTextCursor::End);
        if (!pane->toPlainText().isEmpty()) cur.insertText("\n");
        cur.setCharFormat(fmt);
        cur.insertText(text);
    };

    for (int i = 0; i < maxLen; ++i) {
        QString ol = i < origLines.size() ? origLines[i] : QString();
        QString ml = i < modLines.size()  ? modLines[i]  : QString();
        bool differ = (ol != ml);
        appendLine(m_leftPane,  ol, differ ? diffFmtOrig : normalFmt);
        appendLine(m_rightPane, ml, differ ? diffFmtMod  : normalFmt);
    }
}

void WhatIfDialog::applyTheme(bool isDark)
{
    m_isDark = isDark;
    QString paneBg  = isDark ? "#181825" : "#f5f5f5";
    QString paneFg  = isDark ? "#cdd6f4" : "#1e1e2e";
    QString border  = isDark ? "#313244" : "#d0d0d0";
    QString paneStyle = QString(
        "QPlainTextEdit { background: %1; color: %2; border: 1px solid %3;"
        " font-family: 'Cascadia Code','Consolas',monospace; font-size: 12px; }")
        .arg(paneBg, paneFg, border);
    if (m_leftPane)  m_leftPane->setStyleSheet(paneStyle);
    if (m_rightPane) m_rightPane->setStyleSheet(paneStyle);
}
