#include "core/debugger.h"
#include "core/analysisframe.h"
#include "core/debugbackend.h"
#include "core/theme.h"
#include "core/jsonloader.h"

#include <QRegularExpression>
#include <QJsonArray>
#include <QJsonObject>

using C = Theme::Colors;

#include <QPainter>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFrame>
#include <QDialog>
#include <QScrollArea>
#include <QFile>
#include <QTimer>
#include <QPropertyAnimation>

// ═══════════════════════════════════════════════════════════════════════
// WhatsWrongAnalyzer  (merged from whatswrong.cpp)
// ═══════════════════════════════════════════════════════════════════════

WhatsWrongAnalyzer::WhatsWrongAnalyzer(QObject* parent)
    : QObject(parent)
{
    QJsonArray arr = JsonLoader::loadArray("errors.json", "error_types");
    for (const QJsonValue& v : arr)
        m_errorTypes.append(v.toObject());
}

QList<PossibleCause> WhatsWrongAnalyzer::analyzeError(const QString& errorMessage,
                                                        const QString& /*code*/,
                                                        int crashLine) const
{
    QList<PossibleCause> causes;
    QString msg = errorMessage.toLower();

    for (const QJsonObject& et : m_errorTypes) {
        bool matched = false;
        if (et.contains("match")) {
            matched = msg.contains(et["match"].toString());
        } else if (et.contains("match_any")) {
            for (const QJsonValue& m : et["match_any"].toArray()) {
                if (msg.contains(m.toString())) { matched = true; break; }
            }
        }
        if (!matched) continue;

        PossibleCause c;
        c.title        = et["title"].toString();
        c.relevantLine = crashLine;

        bool patternMatched = false;
        if (et.contains("patterns")) {
            for (const QJsonValue& pv : et["patterns"].toArray()) {
                QJsonObject p = pv.toObject();
                QRegularExpression re(p["regex"].toString());
                auto m = re.match(errorMessage);
                if (m.hasMatch()) {
                    QString expl = p["explanation_template"].toString();
                    QString fix  = p.contains("fix") ? p["fix"].toString()
                                                     : p["fix_template"].toString();
                    for (int i = 1; i <= m.lastCapturedIndex(); ++i) {
                        expl.replace(QString("%%1").arg(i), m.captured(i));
                        fix.replace(QString("%%1").arg(i), m.captured(i));
                    }
                    c.explanation  = expl;
                    c.suggestedFix = fix;
                    patternMatched = true;
                    break;
                }
            }
        }
        if (!patternMatched && et.contains("regex")) {
            QRegularExpression re(et["regex"].toString());
            auto m = re.match(errorMessage);
            if (m.hasMatch()) {
                QString captured = m.captured(et["capture_group"].toInt(1));
                c.explanation  = et["explanation_with_var"].toString().replace("%1", captured);
                c.suggestedFix = et["fix_with_var"].toString().replace("%1", captured);
                patternMatched = true;
            }
        }
        if (!patternMatched) {
            c.explanation  = et["explanation_generic"].toString();
            c.suggestedFix = et["fix_generic"].toString();
        }

        causes.append(c);
    }

    return causes;
}

// ═══════════════════════════════════════════════════════════════════════
// ExecutionRecorder  (merged from executionrecorder.cpp)
// ═══════════════════════════════════════════════════════════════════════

ExecutionRecorder::ExecutionRecorder(QObject* parent)
    : QObject(parent)
{
    m_buffer.resize(MAX_SNAPSHOTS);
}

void ExecutionRecorder::record(int line,
                                const QMap<QString, QVariant>& variables,
                                const QStringList& callStack)
{
    if (variables.size() > MEMORY_WARN_VARS) {
        emit memoryWarning(
            QString("Watch out: %1 variables in scope. Recording may use significant memory.")
                .arg(variables.size()));
    }

    ExecutionSnapshot snap;
    snap.line      = line;
    snap.variables = variables;
    snap.callStack = callStack;

    m_buffer[m_head] = snap;
    m_head = (m_head + 1) % MAX_SNAPSHOTS;
    if (m_count < MAX_SNAPSHOTS)
        ++m_count;

    m_position = 0;
}

bool ExecutionRecorder::stepBack(ExecutionSnapshot& snap)
{
    if (m_position >= m_count - 1)
        return false;

    ++m_position;
    int idx = ((m_head - 1 - m_position) % MAX_SNAPSHOTS + MAX_SNAPSHOTS) % MAX_SNAPSHOTS;
    snap = m_buffer[idx];
    return true;
}

bool ExecutionRecorder::stepForward(ExecutionSnapshot& snap)
{
    if (m_position <= 0)
        return false;

    --m_position;
    int idx = ((m_head - 1 - m_position) % MAX_SNAPSHOTS + MAX_SNAPSHOTS) % MAX_SNAPSHOTS;
    snap = m_buffer[idx];
    return true;
}

void ExecutionRecorder::reset()
{
    m_head     = 0;
    m_count    = 0;
    m_position = 0;
    for (auto& s : m_buffer) {
        s.line = 0;
        s.variables.clear();
        s.callStack.clear();
    }
}

// ═══════════════════════════════════════════════════════════════════════
// DebugEditor
// ═══════════════════════════════════════════════════════════════════════

DebugEditor::DebugEditor(QWidget* parent)
    : QPlainTextEdit(parent)
{
}

QTextBlock DebugEditor::firstBlock() const      { return firstVisibleBlock(); }
QRectF     DebugEditor::blockGeometry(const QTextBlock& b) const { return blockBoundingGeometry(b); }
QRectF     DebugEditor::blockRect(const QTextBlock& b)     const { return blockBoundingRect(b); }
QPointF    DebugEditor::contentOff()            const { return contentOffset(); }
void       DebugEditor::setGutterWidth(int w)         { setViewportMargins(w, 0, 0, 0); }

// ═══════════════════════════════════════════════════════════════════════
// CodeGutter
// ═══════════════════════════════════════════════════════════════════════

CodeGutter::CodeGutter(DebugEditor* editor, QWidget* parent)
    : QWidget(parent), m_editor(editor)
{
    setFixedWidth(40);
    setCursor(Qt::PointingHandCursor);
}

void CodeGutter::setBreakpoints(const QSet<int>& lines)
{
    m_breakpoints = lines;
    update();
}

void CodeGutter::setCurrentLine(int line)
{
    m_currentLine = line;
    update();
}

QSize CodeGutter::sizeHint() const
{
    return QSize(40, 0);
}

void CodeGutter::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(C::bg()));

    QTextBlock block = m_editor->firstBlock();
    int blockNumber = block.blockNumber();
    int top    = (int)m_editor->blockGeometry(block).translated(m_editor->contentOff()).top();
    int bottom = top + (int)m_editor->blockRect(block).height();

    while (block.isValid() && top <= rect().bottom()) {
        if (block.isVisible() && bottom >= rect().top()) {
            int lineNum = blockNumber + 1;

            // Current line highlight
            if (lineNum == m_currentLine) {
                p.fillRect(0, top, width(), bottom - top, QColor(255, 230, 0, 40));
            }

            // Line number text
            p.setPen(lineNum == m_currentLine ? QColor(C::yellow()) : QColor(C::fg3()));
            p.setFont(m_editor->font());
            p.drawText(2, top, width() - 16, bottom - top,
                       Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(lineNum));

            // Breakpoint dot
            if (m_breakpoints.contains(lineNum)) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(C::red()));
                int cy = top + (bottom - top) / 2;
                p.drawEllipse(width() - 12, cy - 5, 10, 10);
            }

            // Current line arrow
            if (lineNum == m_currentLine) {
                p.setPen(QColor(C::green()));
                p.setBrush(QColor(C::green()));
                int cy = top + (bottom - top) / 2;
                QPolygon arrow;
                arrow << QPoint(2, cy - 4) << QPoint(10, cy) << QPoint(2, cy + 4);
                p.drawPolygon(arrow);
            }
        }
        block = block.next();
        top = bottom;
        bottom = top + (int)m_editor->blockRect(block).height();
        ++blockNumber;
    }
}

void CodeGutter::mousePressEvent(QMouseEvent* event)
{
    // Map click Y → block → line number
    QTextBlock block = m_editor->firstBlock();
    int blockNumber = block.blockNumber();
    int top    = (int)m_editor->blockGeometry(block).translated(m_editor->contentOff()).top();
    int bottom = top + (int)m_editor->blockRect(block).height();

    while (block.isValid()) {
        if (event->pos().y() >= top && event->pos().y() < bottom) {
            emit breakpointToggled(blockNumber + 1);
            return;
        }
        block = block.next();
        top = bottom;
        bottom = top + (int)m_editor->blockRect(block).height();
        ++blockNumber;
    }
}

// ═══════════════════════════════════════════════════════════════════════
// DebugCodeView
// ═══════════════════════════════════════════════════════════════════════

DebugCodeView::DebugCodeView(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_editor = new DebugEditor;
    m_editor->setReadOnly(true);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setStyleSheet(
        QString("QPlainTextEdit {"
        " background: %1; color: %2;"
        " font-family: 'Cascadia Code', 'Consolas', monospace;"
        " font-size: 12px; border: none; }"
        "QScrollBar:vertical { background: %1; width: 8px; }"
        "QScrollBar::handle:vertical { background: %3; border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(C::bg(), C::fg(), C::border()));

    m_gutter = new CodeGutter(m_editor, this);
    lay->addWidget(m_gutter);
    lay->addWidget(m_editor, 1);

    connect(m_gutter, &CodeGutter::breakpointToggled, this, [this](int line) {
        QSet<int> bps = m_gutter->breakpoints();
        if (bps.contains(line))
            bps.remove(line);
        else
            bps.insert(line);
        m_gutter->setBreakpoints(bps);
        emit breakpointToggled(line);
    });

    connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
            m_gutter, QOverload<>::of(&QWidget::update));
    connect(m_editor, &QPlainTextEdit::updateRequest, this, &DebugCodeView::updateGutter);
    connect(m_editor, &QPlainTextEdit::blockCountChanged, this,
            [this](int) { updateGutterWidth(); });
    updateGutterWidth();
}

void DebugCodeView::setCode(const QString& code)
{
    m_editor->setPlainText(code);
    m_currentLine = -1;
    m_gutter->setCurrentLine(-1);
}

void DebugCodeView::setCurrentLine(int line)
{
    m_currentLine = line;
    m_gutter->setCurrentLine(line);

    // Highlight line in editor
    QList<QTextEdit::ExtraSelection> extras;
    if (line >= 1) {
        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(QColor(255, 230, 0, 30));
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        QTextCursor cur(m_editor->document()->findBlockByLineNumber(line - 1));
        sel.cursor = cur;
        sel.cursor.clearSelection();
        extras.append(sel);

        // Scroll to line
        m_editor->setTextCursor(cur);
        m_editor->ensureCursorVisible();
        m_editor->setTextCursor(QTextCursor(m_editor->document()));
    }
    m_editor->setExtraSelections(extras);
    m_gutter->update();
}

void DebugCodeView::setBreakpoints(const QSet<int>& lines)
{
    m_gutter->setBreakpoints(lines);
}

QSet<int> DebugCodeView::breakpoints() const
{
    return m_gutter->breakpoints();
}

void DebugCodeView::updateGutterWidth()
{
    m_editor->setGutterWidth(m_gutter->width());
}

void DebugCodeView::updateGutter(const QRect& rect, int dy)
{
    if (dy)
        m_gutter->scroll(0, dy);
    else
        m_gutter->update(0, rect.y(), m_gutter->width(), rect.height());
}

// ═══════════════════════════════════════════════════════════════════════
// WatchPanel
// ═══════════════════════════════════════════════════════════════════════

WatchPanel::WatchPanel(QWidget* parent)
    : QTreeWidget(parent)
{
    setColumnCount(3);
    setHeaderLabels({"Name", "Type", "Value"});
    header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    header()->setSectionResizeMode(2, QHeaderView::Stretch);
    setStyleSheet(
        QString("QTreeWidget {"
        " background: %1; color: %2;"
        " font-family: 'Cascadia Code', 'Consolas', monospace;"
        " font-size: 11px; border: none; }"
        "QTreeWidget::item { padding: 2px 4px; }"
        "QTreeWidget::item:selected { background: %3; }"
        "QHeaderView::section {"
        " background: %1; color: %4; font-size: 10px;"
        " border: none; padding: 3px 6px; border-bottom: 1px solid %3; }")
        .arg(C::bg(), C::fg(), C::bg2(), C::fg2()));
    setRootIsDecorated(false);
    setAlternatingRowColors(true);
}

void WatchPanel::updateVariables(const QMap<QString, QVariant>& vars)
{
    // Track which names changed
    QSet<QString> changed;
    for (auto it = vars.begin(); it != vars.end(); ++it) {
        if (m_lastValues.contains(it.key()) && m_lastValues[it.key()] != it.value())
            changed.insert(it.key());
    }

    QTreeWidget::clear();
    for (auto it = vars.begin(); it != vars.end(); ++it) {
        auto* item = new QTreeWidgetItem(this);
        item->setText(0, it.key());
        item->setText(1, QString(it.value().typeName()));
        item->setText(2, it.value().toString());

        if (changed.contains(it.key())) {
            flashItem(item);
        }
    }
    m_lastValues = vars;
}

void WatchPanel::flashItem(QTreeWidgetItem* item)
{
    item->setBackground(0, QColor(C::yellow()));
    item->setBackground(1, QColor(C::yellow()));
    item->setBackground(2, QColor(C::yellow()));

    QTimer::singleShot(600, this, [this, item]() {
        // item may be gone after clear(); safe because we only schedule during update
        Q_UNUSED(this);
        Q_UNUSED(item);
    });
}

// ═══════════════════════════════════════════════════════════════════════
// DebugFrame
// ═══════════════════════════════════════════════════════════════════════

DebugFrame::DebugFrame(QWidget* parent)
    : AnalysisFrame("Debugger", parent)
{
    m_backend  = new DebugBackend(this);
    m_recorder = new ExecutionRecorder(this);
    m_analyzer = new WhatsWrongAnalyzer(this);

    // Container widget that fills the inherited results area
    auto* container = new QWidget;
    auto* outerLayout = new QVBoxLayout(container);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // ── Toolbar ──────────────────────────────────────────────────────
    auto* toolbar = new QWidget;
    toolbar->setFixedHeight(40);
    toolbar->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;").arg(C::bg2(), C::bg2()));
    buildToolbar(toolbar);
    outerLayout->addWidget(toolbar);

    // ── Main 4-pane split ────────────────────────────────────────────
    auto* hSplit = new QSplitter(Qt::Horizontal);
    hSplit->setHandleWidth(1);
    hSplit->setStyleSheet(QString("QSplitter::handle { background: %1; }").arg(C::bg2()));

    // Left column: code view (top) + call stack (bottom)
    auto* leftSplit = new QSplitter(Qt::Vertical);
    leftSplit->setHandleWidth(1);
    leftSplit->setStyleSheet(QString("QSplitter::handle { background: %1; }").arg(C::bg2()));

    m_codeView = new DebugCodeView;
    leftSplit->addWidget(m_codeView);

    // Call stack panel
    auto* callStackWrapper = new QWidget;
    callStackWrapper->setStyleSheet(QString("background: %1;").arg(C::bg()));
    auto* csLayout = new QVBoxLayout(callStackWrapper);
    csLayout->setContentsMargins(0, 0, 0, 0);
    csLayout->setSpacing(0);

    auto* csHeader = new QLabel("  Call Stack");
    csHeader->setFixedHeight(22);
    csHeader->setStyleSheet(
        QString("QLabel { background: %1; color: %2; font-size: 10px;"
        " border-bottom: 1px solid %3; padding: 0 6px; }")
        .arg(C::bg(), C::fg2(), C::bg2()));
    csLayout->addWidget(csHeader);

    m_callStack = new QListWidget;
    m_callStack->setStyleSheet(
        QString("QListWidget {"
        " background: %1; color: %2;"
        " font-family: 'Cascadia Code', 'Consolas', monospace;"
        " font-size: 11px; border: none; }"
        "QListWidget::item { padding: 3px 8px; }"
        "QListWidget::item:selected { background: %3; color: %4; }"
        "QListWidget::item:hover { background: %3; }")
        .arg(C::bg(), C::fg(), C::bg2(), C::accent()));
    csLayout->addWidget(m_callStack, 1);
    leftSplit->addWidget(callStackWrapper);
    leftSplit->setSizes({600, 200});

    hSplit->addWidget(leftSplit);

    // Right column: watch panel (top) + console (bottom)
    auto* rightSplit = new QSplitter(Qt::Vertical);
    rightSplit->setHandleWidth(1);
    rightSplit->setStyleSheet(QString("QSplitter::handle { background: %1; }").arg(C::bg2()));

    // Watch panel
    auto* watchWrapper = new QWidget;
    watchWrapper->setStyleSheet(QString("background: %1;").arg(C::bg()));
    auto* watchLayout = new QVBoxLayout(watchWrapper);
    watchLayout->setContentsMargins(0, 0, 0, 0);
    watchLayout->setSpacing(0);

    auto* watchHeader = new QLabel("  Watch Panel");
    watchHeader->setFixedHeight(22);
    watchHeader->setStyleSheet(
        QString("QLabel { background: %1; color: %2; font-size: 10px;"
        " border-bottom: 1px solid %3; padding: 0 6px; }")
        .arg(C::bg(), C::fg2(), C::bg2()));
    watchLayout->addWidget(watchHeader);

    m_watchPanel = new WatchPanel;
    watchLayout->addWidget(m_watchPanel, 1);
    rightSplit->addWidget(watchWrapper);

    // Console
    auto* consoleWrapper = new QWidget;
    consoleWrapper->setStyleSheet(QString("background: %1;").arg(C::bg()));
    auto* consoleLayout = new QVBoxLayout(consoleWrapper);
    consoleLayout->setContentsMargins(0, 0, 0, 0);
    consoleLayout->setSpacing(0);

    auto* consoleHeader = new QLabel("  Debug Console");
    consoleHeader->setFixedHeight(22);
    consoleHeader->setStyleSheet(
        QString("QLabel { background: %1; color: %2; font-size: 10px;"
        " border-bottom: 1px solid %3; border-top: 1px solid %3; padding: 0 6px; }")
        .arg(C::bg(), C::fg2(), C::bg2()));
    consoleLayout->addWidget(consoleHeader);

    m_console = new QPlainTextEdit;
    m_console->setReadOnly(true);
    m_console->setStyleSheet(
        QString("QPlainTextEdit {"
        " background: %1; color: %2;"
        " font-family: 'Cascadia Code', 'Consolas', monospace;"
        " font-size: 11px; border: none; padding: 4px; }"
        "QScrollBar:vertical { background: %1; width: 8px; }"
        "QScrollBar::handle:vertical { background: %3; border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(C::bg(), C::green(), C::border()));
    consoleLayout->addWidget(m_console, 1);
    rightSplit->addWidget(consoleWrapper);
    rightSplit->setSizes({400, 200});

    hSplit->addWidget(rightSplit);
    hSplit->setSizes({700, 350});

    outerLayout->addWidget(hSplit, 1);

    // ── "What Could Be Wrong?" panel (hidden by default) ─────────────
    buildWhatsWrongPanel();
    outerLayout->addWidget(m_whatsWrongPanel);
    m_whatsWrongPanel->hide();

    // Add the entire content container into the inherited results area
    addResultWidget(container);
    setStatus("Debugger ready");

    // ── Wire backend signals ─────────────────────────────────────────
    connect(m_backend, &DebugBackend::lineChanged,
            this, &DebugFrame::onLineChanged);
    connect(m_backend, &DebugBackend::variableUpdate,
            this, &DebugFrame::onVariableUpdate);
    connect(m_backend, &DebugBackend::callStackChanged,
            this, &DebugFrame::onCallStackChanged);
    connect(m_backend, &DebugBackend::output,
            this, &DebugFrame::onOutput);
    connect(m_backend, &DebugBackend::crashed,
            this, &DebugFrame::onCrash);

    // ── Connect codeview breakpoints to backend ──────────────────────
    connect(m_codeView, &DebugCodeView::breakpointToggled, this, [this]() {
        m_backend->setBreakpoints(m_codeView->breakpoints());
    });

    // ── Call stack click → jump to frame ────────────────────────────
    connect(m_callStack, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        // Frame text: "functionName | file | line"
        QString text = item->text();
        QStringList parts = text.split(" | ");
        if (parts.size() >= 3) {
            bool ok;
            int line = parts[2].trimmed().toInt(&ok);
            if (ok) m_codeView->setCurrentLine(line);
        }
    });
}

void DebugFrame::buildToolbar(QWidget* toolbar)
{
    auto* lay = new QHBoxLayout(toolbar);
    lay->setContentsMargins(8, 0, 8, 0);
    lay->setSpacing(4);

    auto makeBtn = [&](const QString& icon, const QString& tip) -> QPushButton* {
        auto* btn = new QPushButton(icon);
        btn->setFixedSize(32, 28);
        btn->setToolTip(tip);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            QString("QPushButton { background: %1; color: %2;"
            " border: 1px solid %3; border-radius: 5px;"
            " font-size: 14px; padding: 0; }"
            "QPushButton:hover { background: %4; }"
            "QPushButton:pressed { background: %3; }"
            "QPushButton:disabled { color: %5; background: %1; border-color: %1; }")
            .arg(C::bg2(), C::fg(), C::border(), C::bg3(), C::fg3()));
        return btn;
    };

    m_continueBtn = makeBtn(QString::fromUtf8("\xe2\x96\xb6"), "Continue (F5)");
    m_stepOverBtn = makeBtn(QString::fromUtf8("\xe2\x8f\xad"), "Step Over (F10)");
    m_stepIntoBtn = makeBtn(QString::fromUtf8("\xe2\xac\x87"), "Step Into (F11)");
    m_stepOutBtn  = makeBtn(QString::fromUtf8("\xe2\xac\x86"), "Step Out (Shift+F11)");
    m_backBtn     = makeBtn(QString::fromUtf8("\xe2\x86\xa9"), "Step Back");
    m_stopBtn     = makeBtn(QString::fromUtf8("\xe2\x8f\xb9"), "Stop (Shift+F5)");
    m_restartBtn  = makeBtn(QString::fromUtf8("\xe2\x86\xa9"), "Restart (Ctrl+Shift+F5)");

    lay->addWidget(m_continueBtn);
    lay->addWidget(m_stepOverBtn);
    lay->addWidget(m_stepIntoBtn);
    lay->addWidget(m_stepOutBtn);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::VLine);
    sep->setFixedHeight(16);
    sep->setStyleSheet(QString("color: %1;").arg(C::border()));
    lay->addWidget(sep);

    lay->addWidget(m_backBtn);
    lay->addWidget(m_stopBtn);
    lay->addWidget(m_restartBtn);

    lay->addStretch();

    // Debug help button
    auto* helpBtn = new QPushButton("?");
    helpBtn->setFixedSize(26, 26);
    helpBtn->setCursor(Qt::PointingHandCursor);
    helpBtn->setStyleSheet(
        QString("QPushButton { background: transparent; color: %1;"
        " border-radius: 13px; font-size: 14px; border: none; }"
        "QPushButton:hover { color: %2; }")
        .arg(C::fg2(), C::fg()));
    connect(helpBtn, &QPushButton::clicked, this, [this]() {
        auto* dlg = new QDialog(this);
        dlg->setWindowTitle("Using the Debugger");
        dlg->setModal(true);
        dlg->setMinimumWidth(480);
        dlg->setStyleSheet(QString("background: %1;").arg(C::bg()));
        auto* lay = new QVBoxLayout(dlg);
        lay->setContentsMargins(22, 18, 22, 18);
        lay->setSpacing(12);

        auto* title = new QLabel("How to Debug Your Code");
        title->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: bold;").arg(C::fg()));
        lay->addWidget(title);

        const QString helpText =
            "<p style='color:#a6adc8; font-size:12px;'>"
            "<b style='color:#cdd6f4;'>What is debugging?</b><br>"
            "Debugging means finding and fixing bugs — parts of your code that don't behave as intended."
            "</p>"
            "<p style='color:#a6adc8; font-size:12px;'>"
            "<b style='color:#cdd6f4;'>Step controls:</b><br>"
            "<b style='color:#a6e3a1;'>&#9654; Continue</b> — run until the next breakpoint<br>"
            "<b style='color:#a6e3a1;'>&#9197; Step Over</b> — run the current line, move to the next<br>"
            "<b style='color:#a6e3a1;'>&#8681; Step Into</b> — go inside the function being called<br>"
            "<b style='color:#a6e3a1;'>&#8679; Step Out</b> — finish the current function and return to the caller<br>"
            "<b style='color:#a6e3a1;'>&#8617; Step Back</b> — rewind to the previous recorded step<br>"
            "<b style='color:#f38ba8;'>&#9209; Stop</b> — end the debug session"
            "</p>"
            "<p style='color:#a6adc8; font-size:12px;'>"
            "<b style='color:#cdd6f4;'>Breakpoints:</b> Click the line number gutter (left margin) to add a red dot. "
            "The debugger will pause there automatically."
            "</p>"
            "<p style='color:#a6adc8; font-size:12px;'>"
            "<b style='color:#cdd6f4;'>Watch Panel:</b> Shows variable values. Values flash yellow when they change."
            "</p>"
            "<p style='color:#a6adc8; font-size:12px;'>"
            "<b style='color:#cdd6f4;'>Reverse debugging:</b> When a crash happens, use Step Back to rewind "
            "through recorded steps and find what went wrong."
            "</p>";

        auto* text = new QLabel(helpText);
        text->setWordWrap(true);
        text->setTextFormat(Qt::RichText);
        lay->addWidget(text);

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
    });
    lay->addWidget(helpBtn);

    // Wire buttons
    connect(m_continueBtn, &QPushButton::clicked, this, &DebugFrame::onContinue);
    connect(m_stepOverBtn, &QPushButton::clicked, this, &DebugFrame::onStepOver);
    connect(m_stepIntoBtn, &QPushButton::clicked, this, &DebugFrame::onStepInto);
    connect(m_stepOutBtn,  &QPushButton::clicked, this, &DebugFrame::onStepOut);
    connect(m_backBtn,     &QPushButton::clicked, this, &DebugFrame::onStepBack);
    connect(m_stopBtn,     &QPushButton::clicked, this, &DebugFrame::onStop);
    connect(m_restartBtn,  &QPushButton::clicked, this, &DebugFrame::onRestart);
}

void DebugFrame::buildWhatsWrongPanel()
{
    m_whatsWrongPanel = new QWidget;
    m_whatsWrongPanel->setFixedHeight(120);
    m_whatsWrongPanel->setStyleSheet(
        QString("background: %1; border-top: 2px solid %2;").arg(C::bg2(), C::red()));

    auto* lay = new QVBoxLayout(m_whatsWrongPanel);
    lay->setContentsMargins(12, 8, 12, 8);
    lay->setSpacing(6);

    auto* headerRow = new QHBoxLayout;
    auto* wwTitle = new QLabel("What Could Be Wrong?");
    wwTitle->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold;").arg(C::red()));
    headerRow->addWidget(wwTitle);
    headerRow->addStretch();

    auto* wwClose = new QPushButton(QChar(0x2715));
    wwClose->setFixedSize(16, 16);
    wwClose->setStyleSheet(
        QString("QPushButton { background: transparent; color: %1; border: none; font-size: 11px; }"
        "QPushButton:hover { color: %2; }")
        .arg(C::fg3(), C::fg()));
    connect(wwClose, &QPushButton::clicked, this, &DebugFrame::hideWhatsWrongPanel);
    headerRow->addWidget(wwClose);
    lay->addLayout(headerRow);

    m_whatsWrongContent = new QLabel;
    m_whatsWrongContent->setWordWrap(true);
    m_whatsWrongContent->setStyleSheet(QString("color: %1; font-size: 11px;").arg(C::fg()));
    m_whatsWrongContent->setTextFormat(Qt::RichText);
    lay->addWidget(m_whatsWrongContent);
}

void DebugFrame::setTargetFile(const QString& filePath, const QString& lang)
{
    m_targetFile = filePath;
    m_lang = lang;

    QFile f(filePath);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_codeView->setCode(QString::fromUtf8(f.readAll()));
        f.close();
    }
    m_statusLabel->setText(QString("Target: %1").arg(filePath));
}

void DebugFrame::startDebugging()
{
    m_console->clear();
    static_cast<QTreeWidget*>(m_watchPanel)->clear();
    m_callStack->clear();
    m_recorder->reset();
    m_crashed = false;
    hideWhatsWrongPanel();

    m_statusLabel->setText("Starting debugger...");
    m_backend->startSession(m_targetFile, m_codeView->breakpoints());
}

void DebugFrame::stopDebugging()
{
    m_backend->stop();
    m_statusLabel->setText("Stopped");
}

void DebugFrame::onContinue()  { m_backend->sendCommand("continue"); }
void DebugFrame::onStepOver()  { m_backend->sendCommand("step"); }
void DebugFrame::onStepInto()  { m_backend->sendCommand("into"); }
void DebugFrame::onStepOut()   { m_backend->sendCommand("out"); }
void DebugFrame::onStop()      { stopDebugging(); }

void DebugFrame::onRestart()
{
    stopDebugging();
    QTimer::singleShot(200, this, &DebugFrame::startDebugging);
}

void DebugFrame::onStepBack()
{
    ExecutionSnapshot snap;
    if (m_recorder->stepBack(snap)) {
        m_codeView->setCurrentLine(snap.line);
        m_watchPanel->updateVariables(snap.variables);
        m_callStack->clear();
        for (const QString& frame : snap.callStack)
            m_callStack->addItem(frame);
        m_statusLabel->setText(QString("Step back — line %1").arg(snap.line));
    }
}

void DebugFrame::onLineChanged(const QString& /*file*/, int line)
{
    m_codeView->setCurrentLine(line);
    m_statusLabel->setText(QString("Paused at line %1").arg(line));
}

void DebugFrame::onVariableUpdate(const QMap<QString, QVariant>& vars)
{
    m_watchPanel->updateVariables(vars);
    // Record snapshot
    m_recorder->record(m_codeView->breakpoints().isEmpty() ? 0 : *m_codeView->breakpoints().begin(),
                       vars, {});
}

void DebugFrame::onCallStackChanged(const QStringList& frames)
{
    m_callStack->clear();
    for (const QString& frame : frames)
        m_callStack->addItem(frame);
}

void DebugFrame::onOutput(const QString& text)
{
    m_console->moveCursor(QTextCursor::End);
    m_console->insertPlainText(text);
    m_console->ensureCursorVisible();
}

void DebugFrame::onCrash(const QString& errorMsg, int crashLine)
{
    m_crashed = true;
    m_statusLabel->setText(QString("Crashed at line %1").arg(crashLine));
    m_console->appendPlainText("\n[CRASH] " + errorMsg);

    // Show What Could Be Wrong
    QFile f(m_targetFile);
    QString code;
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        code = QString::fromUtf8(f.readAll());
        f.close();
    }
    showWhatsWrongPanel(errorMsg, crashLine);
}

void DebugFrame::showWhatsWrongPanel(const QString& error, int line)
{
    QFile f(m_targetFile);
    QString code;
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        code = QString::fromUtf8(f.readAll());
        f.close();
    }

    QList<PossibleCause> causes = m_analyzer->analyzeError(error, code, line);

    QString html;
    int shown = qMin(causes.size(), 3);
    for (int i = 0; i < shown; ++i) {
        const auto& c = causes[i];
        html += QString("<b style='color:#f38ba8;'>%1</b> — %2")
                    .arg(c.title, c.explanation);
        if (!c.suggestedFix.isEmpty())
            html += QString(" <span style='color:#a6e3a1;'>Fix: %1</span>")
                        .arg(c.suggestedFix);
        if (i < shown - 1) html += "<br>";
    }
    if (html.isEmpty())
        html = "<span style='color:#a6adc8;'>No specific pattern matched. Check the console for the full error.</span>";

    m_whatsWrongContent->setText(html);
    m_whatsWrongPanel->show();
}

void DebugFrame::hideWhatsWrongPanel()
{
    m_whatsWrongPanel->hide();
}

void DebugFrame::applyTheme(bool isDark)
{
    m_isDark = isDark;
    // Theme support — dark-only for now (matches the rest of the app defaults)
}
