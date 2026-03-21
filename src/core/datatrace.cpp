#include "core/datatrace.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryFile>
#include <QDir>
#include <QScrollBar>

#ifdef Q_OS_WIN
#include <windows.h>
static void hideConsoleWindow(QProcess& p) {
    p.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
}
#else
static void hideConsoleWindow(QProcess&) {}
#endif

// ============================================================================
// TraceCard
// ============================================================================

TraceCard::TraceCard(const TraceStep& step, QWidget* parent)
    : QWidget(parent), m_step(step)
{
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(54);

    QString bg    = step.isError ? "#3d1f1f" : "#2a2a3c";
    QString border = step.isError ? "#f38ba8" : "#45475a";
    QString lineColor = "#89b4fa";
    QString actionColor = step.isError ? "#f38ba8" : "#cdd6f4";
    QString valueColor = step.isError ? "#f38ba8" : "#a6e3a1";

    setStyleSheet(QString(
        "TraceCard { background: %1; border: 1px solid %2;"
        " border-radius: 6px; }"
        "TraceCard:hover { border-color: #89b4fa; }"
    ).arg(bg, border));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(12);

    // Line number badge
    auto* lineBadge = new QLabel(QString("Line %1").arg(step.lineNumber));
    lineBadge->setFixedWidth(54);
    lineBadge->setAlignment(Qt::AlignCenter);
    lineBadge->setStyleSheet(QString(
        "QLabel { background: #313244; color: %1; border-radius: 4px;"
        " font-size: 11px; font-weight: bold; padding: 2px 4px; }"
    ).arg(lineColor));
    layout->addWidget(lineBadge);

    // Separator
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::VLine);
    sep->setStyleSheet("color: #45475a;");
    layout->addWidget(sep);

    // Action description
    auto* actionLabel = new QLabel(step.action);
    actionLabel->setStyleSheet(QString(
        "QLabel { color: %1; font-size: 12px; }"
    ).arg(actionColor));
    actionLabel->setWordWrap(false);
    layout->addWidget(actionLabel, 1);

    // Value (right side)
    if (!step.value.isEmpty()) {
        auto* valueSep = new QFrame;
        valueSep->setFrameShape(QFrame::VLine);
        valueSep->setStyleSheet("color: #45475a;");
        layout->addWidget(valueSep);

        auto* valueLabel = new QLabel(step.value);
        valueLabel->setStyleSheet(QString(
            "QLabel { color: %1; font-family: 'Cascadia Code', 'Consolas', monospace;"
            " font-size: 11px; }"
        ).arg(valueColor));
        valueLabel->setMaximumWidth(200);
        valueLabel->setWordWrap(false);
        layout->addWidget(valueLabel);
    }
}

void TraceCard::mousePressEvent(QMouseEvent* event)
{
    QWidget::mousePressEvent(event);
    if (event->button() == Qt::LeftButton)
        emit lineJumpRequested(m_step.lineNumber);
}

// ============================================================================
// DataTraceFrame
// ============================================================================

DataTraceFrame::DataTraceFrame(QWidget* parent)
    : QWidget(parent)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    setStyleSheet("background: #1e1e2e; color: #cdd6f4;");

    // ── Title bar ─────────────────────────────────────────────────────────────
    auto* titleBar = new QWidget;
    titleBar->setFixedHeight(44);
    titleBar->setStyleSheet("background: #2a2a3c; border-bottom: 1px solid #313244;");
    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(12, 0, 12, 0);
    titleLayout->setSpacing(8);

    auto* backBtn = new QPushButton("← Back to Editor");
    backBtn->setFixedHeight(28);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; font-size: 12px; padding: 0 12px; }"
        "QPushButton:hover { background: #45475a; }");
    connect(backBtn, &QPushButton::clicked, this, &DataTraceFrame::backToEditor);
    titleLayout->addWidget(backBtn);

    titleLayout->addStretch();

    auto* titleLabel = new QLabel("Trace the Data");
    titleLabel->setStyleSheet("color: #cdd6f4; font-size: 14px; font-weight: bold;");
    titleLayout->addWidget(titleLabel);

    titleLayout->addStretch();

    m_liveTraceBtn = new QPushButton("Run Live Trace");
    m_liveTraceBtn->setFixedHeight(28);
    m_liveTraceBtn->setCursor(Qt::PointingHandCursor);
    m_liveTraceBtn->setStyleSheet(
        "QPushButton { background: #89b4fa; color: #1e1e2e; font-weight: bold;"
        " border-radius: 4px; font-size: 12px; padding: 0 14px; }"
        "QPushButton:hover { background: #a0c4fb; }"
        "QPushButton:disabled { background: #45475a; color: #6c7086; }");
    m_liveTraceBtn->setEnabled(false);
    connect(m_liveTraceBtn, &QPushButton::clicked, this, &DataTraceFrame::onRunLiveTrace);
    titleLayout->addWidget(m_liveTraceBtn);

    outerLayout->addWidget(titleBar);

    // ── Variable input bar ────────────────────────────────────────────────────
    auto* inputBar = new QWidget;
    inputBar->setFixedHeight(46);
    inputBar->setStyleSheet("background: #181825; border-bottom: 1px solid #313244;");
    auto* inputLayout = new QHBoxLayout(inputBar);
    inputLayout->setContentsMargins(16, 8, 16, 8);
    inputLayout->setSpacing(8);

    auto* varLabel = new QLabel("Variable:");
    varLabel->setStyleSheet("color: #a6adc8; font-size: 12px;");
    inputLayout->addWidget(varLabel);

    m_varInput = new QLineEdit;
    m_varInput->setPlaceholderText("e.g. total, user_input, result ...");
    m_varInput->setFixedHeight(28);
    m_varInput->setStyleSheet(
        "QLineEdit { background: #2a2a3c; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; font-size: 12px; padding: 0 8px; }"
        "QLineEdit:focus { border-color: #89b4fa; }");
    connect(m_varInput, &QLineEdit::returnPressed, this, &DataTraceFrame::onTrace);
    inputLayout->addWidget(m_varInput, 1);

    m_traceBtn = new QPushButton("Trace");
    m_traceBtn->setFixedHeight(28);
    m_traceBtn->setCursor(Qt::PointingHandCursor);
    m_traceBtn->setStyleSheet(
        "QPushButton { background: #a6e3a1; color: #1e1e2e; font-weight: bold;"
        " border-radius: 4px; font-size: 12px; padding: 0 20px; }"
        "QPushButton:hover { background: #b9f0b4; }");
    connect(m_traceBtn, &QPushButton::clicked, this, &DataTraceFrame::onTrace);
    inputLayout->addWidget(m_traceBtn);

    outerLayout->addWidget(inputBar);

    // ── Status label ──────────────────────────────────────────────────────────
    m_statusLabel = new QLabel("Enter a variable name and click Trace.");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(
        "QLabel { color: #6c7086; font-size: 12px; padding: 8px; background: #1e1e2e; }");
    outerLayout->addWidget(m_statusLabel);

    // ── Cards scroll area ──────────────────────────────────────────────────────
    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setStyleSheet(
        "QScrollArea { border: none; background: #1e1e2e; }"
        "QScrollBar:vertical { background: #181825; width: 8px; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background: #45475a; border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    m_cardsArea = new QWidget;
    m_cardsArea->setStyleSheet("background: #1e1e2e;");
    m_cardsLayout = new QVBoxLayout(m_cardsArea);
    m_cardsLayout->setContentsMargins(16, 12, 16, 12);
    m_cardsLayout->setSpacing(6);
    m_cardsLayout->addStretch();

    m_scroll->setWidget(m_cardsArea);
    outerLayout->addWidget(m_scroll, 1);
}

void DataTraceFrame::setCode(const QString& code, const QString& language, const QString& filePath)
{
    m_code     = code;
    m_language = language;
    m_filePath = filePath;

    bool isPython = (language == "python");
    m_liveTraceBtn->setEnabled(isPython && !filePath.isEmpty());

    clearCards();
    m_statusLabel->setText("Enter a variable name and click Trace.");
}

void DataTraceFrame::setVariable(const QString& varName)
{
    if (m_varInput)
        m_varInput->setText(varName);
}

// ── Static trace ──────────────────────────────────────────────────────────────

void DataTraceFrame::onTrace()
{
    QString varName = m_varInput->text().trimmed();
    if (varName.isEmpty()) {
        m_statusLabel->setText("Please enter a variable name.");
        return;
    }

    QStringList lines = m_code.split('\n');
    QList<TraceStep> steps = staticTrace(varName, lines, m_language);

    if (steps.isEmpty()) {
        clearCards();
        m_statusLabel->setText(
            QString("No assignments or uses of '%1' found in this file.").arg(varName));
        return;
    }

    m_statusLabel->setText(
        QString("Found %1 step(s) for '%2'  (static analysis — click 'Run Live Trace' for real values)")
        .arg(steps.size()).arg(varName));

    populateCards(steps);
}

QList<TraceStep> DataTraceFrame::staticTrace(const QString& varName,
                                              const QStringList& lines,
                                              const QString& lang) const
{
    QList<TraceStep> steps;

    // Patterns we recognise (language-agnostic heuristics):
    // 1. Assignment:          varName = ...
    // 2. Augmented assign:    varName += ...  varName -= ... etc.
    // 3. Function param:      def foo(varName, ...)  /  function foo(varName)
    // 4. Passed to call:      foo(varName)
    // 5. Return:              return varName
    // 6. Cast attempt:        int(varName)  float(varName)  str(varName)

    const QString escaped = QRegularExpression::escape(varName);

    // Assignment
    QRegularExpression reAssign(
        QString("^\\s*%1\\s*=(?!=)").arg(escaped));
    // Augmented assign
    QRegularExpression reAugAssign(
        QString("^\\s*%1\\s*[+\\-*/%%&|^]=").arg(escaped));
    // For-loop variable
    QRegularExpression reFor(
        QString("\\bfor\\s+%1\\b").arg(escaped));
    // Function definition with this param
    QRegularExpression reParam(
        QString("\\bdef\\s+\\w+\\s*\\([^)]*\\b%1\\b").arg(escaped));
    // Passed as argument
    QRegularExpression rePassed(
        QString("\\b\\w+\\s*\\([^)]*\\b%1\\b").arg(escaped));
    // Return statement
    QRegularExpression reReturn(
        QString("\\breturn\\s+.*\\b%1\\b").arg(escaped));
    // Type cast
    QRegularExpression reCast(
        QString("\\b(int|float|str|bool|list|dict|tuple|set)\\s*\\(\\s*%1\\s*\\)").arg(escaped));

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        int lineNum = i + 1;

        TraceStep step;
        step.lineNumber = lineNum;

        // Extract value from assignment (everything after '=')
        auto extractValue = [&]() -> QString {
            int eq = line.indexOf('=');
            if (eq >= 0) {
                QString rhs = line.mid(eq + 1).trimmed();
                if (rhs.length() > 60) rhs = rhs.left(57) + "...";
                return rhs;
            }
            return {};
        };

        bool matched = false;

        if (reAssign.match(line).hasMatch()) {
            step.action = QString("Assigned  →  %1 = %2").arg(varName, extractValue());
            matched = true;
        } else if (reAugAssign.match(line).hasMatch()) {
            // figure out which operator
            QRegularExpression reOp(
                QString("\\b%1\\s*([+\\-*/%%&|^]=)").arg(escaped));
            auto m = reOp.match(line);
            QString op = m.hasMatch() ? m.captured(1) : "op=";
            step.action = QString("Modified  (%1)  →  %2").arg(op, extractValue());
            matched = true;
        } else if (reFor.match(line).hasMatch()) {
            step.action = QString("Loop variable  —  iterates over %1")
                          .arg(line.mid(line.indexOf("in ") + 3).trimmed());
            matched = true;
        } else if (lang == "python" && reParam.match(line).hasMatch()) {
            // extract function name
            QRegularExpression reFnName("\\bdef\\s+(\\w+)");
            auto m = reFnName.match(line);
            step.action = QString("Parameter in function  %1()")
                          .arg(m.hasMatch() ? m.captured(1) : "?");
            matched = true;
        } else if (reCast.match(line).hasMatch()) {
            auto m = reCast.match(line);
            step.action = QString("Cast to %1").arg(m.captured(1));
            // Note potential failure if this is a cast from string
            if (line.contains("int(") || line.contains("float(")) {
                step.isError = false;  // can't tell statically; note it
                step.value = "May raise ValueError if value is non-numeric";
            }
            matched = true;
        } else if (reReturn.match(line).hasMatch()) {
            step.action = QString("Returned from function");
            matched = true;
        } else if (rePassed.match(line).hasMatch()) {
            // Get function name being called
            QRegularExpression reFnCall("(\\w+)\\s*\\(");
            auto m = reFnCall.match(line);
            QString fnName = m.hasMatch() ? m.captured(1) : "function";
            // Skip keywords that look like calls
            static QStringList kw = {"if","while","for","return","print","assert","elif"};
            if (!kw.contains(fnName)) {
                step.action = QString("Passed to  %1()").arg(fnName);
                matched = true;
            }
        }

        if (matched)
            steps.append(step);
    }

    return steps;
}

// ── Live trace (Python sys.settrace) ──────────────────────────────────────────

void DataTraceFrame::onRunLiveTrace()
{
    QString varName = m_varInput->text().trimmed();
    if (varName.isEmpty()) {
        m_statusLabel->setText("Enter a variable name first.");
        return;
    }
    if (m_filePath.isEmpty() || m_language != "python") {
        m_statusLabel->setText("Live trace requires a saved Python file.");
        return;
    }

    if (m_liveProcess && m_liveProcess->state() != QProcess::NotRunning) {
        m_liveProcess->kill();
        m_liveProcess->waitForFinished(1000);
    }

    // Build the tracer script inline as a temp file
    QString tracerCode = QString(R"(
import sys, json

_var_name = %1
_trace_steps = []

def _tracer(frame, event, arg):
    if event == 'call':
        return _tracer
    if event in ('line', 'return', 'exception'):
        val = frame.f_locals.get(_var_name, None)
        if val is not None:
            try:
                val_str = repr(val)
                if len(val_str) > 80:
                    val_str = val_str[:77] + '...'
            except Exception:
                val_str = '<unprintable>'
            _trace_steps.append({
                'line': frame.f_lineno,
                'event': event,
                'value': val_str,
                'file': frame.f_code.co_filename,
            })
    return _tracer

sys.settrace(_tracer)
try:
    with open(%2, 'r', encoding='utf-8') as _f:
        exec(compile(_f.read(), %2, 'exec'), {})
except SystemExit:
    pass
except Exception as _e:
    pass
finally:
    sys.settrace(None)

print(json.dumps(_trace_steps))
)").arg(
        QString("'%1'").arg(varName),
        QString("'%1'").arg(m_filePath.replace('\\', "/"))
    );

    QTemporaryFile* tmp = new QTemporaryFile(this);
    tmp->setFileTemplate(QDir::tempPath() + "/cc_tracer_XXXXXX.py");
    if (!tmp->open()) {
        m_statusLabel->setText("Failed to create temporary tracer script.");
        return;
    }
    tmp->write(tracerCode.toUtf8());
    tmp->flush();
    QString tracerPath = tmp->fileName();

    m_liveOutput.clear();
    m_statusLabel->setText("Running live trace...");
    m_liveTraceBtn->setEnabled(false);

    if (!m_liveProcess) {
        m_liveProcess = new QProcess(this);
        hideConsoleWindow(*m_liveProcess);
        m_liveProcess->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_liveProcess, &QProcess::readyRead,
                this, &DataTraceFrame::onLiveTraceOutput);
        connect(m_liveProcess,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &DataTraceFrame::onLiveTraceFinished);
    }

    // Try py, then python3, then python
    QStringList pyExes = {"py", "python3", "python"};
    m_liveProcess->setProperty("tracerPath", tracerPath);
    m_liveProcess->setProperty("tmpObj", QVariant::fromValue(static_cast<QObject*>(tmp)));
    m_liveProcess->start(pyExes.first(), {tracerPath});
    if (!m_liveProcess->waitForStarted(2000)) {
        m_liveProcess->start(pyExes[1], {tracerPath});
        if (!m_liveProcess->waitForStarted(2000))
            m_liveProcess->start(pyExes[2], {tracerPath});
    }
}

void DataTraceFrame::onLiveTraceOutput()
{
    m_liveOutput += QString::fromUtf8(m_liveProcess->readAll());
}

void DataTraceFrame::onLiveTraceFinished(int /*exitCode*/, QProcess::ExitStatus /*status*/)
{
    m_liveTraceBtn->setEnabled(m_language == "python" && !m_filePath.isEmpty());

    // Clean up temp file
    if (auto* tmp = qobject_cast<QTemporaryFile*>(
            m_liveProcess->property("tmpObj").value<QObject*>())) {
        tmp->deleteLater();
    }

    // Parse JSON output
    // The last line should be the JSON array
    QStringList outLines = m_liveOutput.split('\n', Qt::SkipEmptyParts);
    QString jsonLine;
    for (int i = outLines.size() - 1; i >= 0; --i) {
        if (outLines[i].trimmed().startsWith('[')) {
            jsonLine = outLines[i].trimmed();
            break;
        }
    }

    if (jsonLine.isEmpty()) {
        m_statusLabel->setText("Live trace complete — no values captured. "
                               "Does the script run without input?");
        return;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonLine.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        m_statusLabel->setText("Live trace complete — could not parse output.");
        return;
    }

    QList<TraceStep> steps;
    const QJsonArray arr = doc.array();
    for (const QJsonValue& v : arr) {
        QJsonObject obj = v.toObject();
        TraceStep step;
        step.lineNumber = obj["line"].toInt();
        step.value      = obj["value"].toString();
        QString event   = obj["event"].toString();
        if (event == "return")
            step.action = "Returned with value";
        else if (event == "exception")
            step.action = "Exception raised at this line";
        else
            step.action = QString("Live value at line %1").arg(step.lineNumber);
        step.isError = (event == "exception");
        steps.append(step);
    }

    QString varName = m_varInput->text().trimmed();
    m_statusLabel->setText(
        QString("Live trace: %1 captured event(s) for '%2'").arg(steps.size()).arg(varName));
    populateCards(steps);
}

// ── UI helpers ────────────────────────────────────────────────────────────────

void DataTraceFrame::clearCards()
{
    // Remove all children except the trailing stretch
    while (m_cardsLayout->count() > 1) {
        QLayoutItem* item = m_cardsLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}

void DataTraceFrame::populateCards(const QList<TraceStep>& steps)
{
    clearCards();

    for (const TraceStep& step : steps) {
        auto* card = new TraceCard(step, m_cardsArea);
        connect(card, &TraceCard::lineJumpRequested, this, &DataTraceFrame::jumpToLine);
        m_cardsLayout->insertWidget(m_cardsLayout->count() - 1, card);
    }

    m_scroll->verticalScrollBar()->setValue(0);
}
