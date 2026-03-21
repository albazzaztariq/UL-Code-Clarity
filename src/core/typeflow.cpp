#include "core/typeflow.h"
#include "core/theme.h"

using namespace Theme::Css;

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QFontMetrics>
#include <QApplication>
#include <algorithm>

// ============================================================================
// Layout constants
// ============================================================================
static const int NODE_W          = 180;
static const int NODE_H_BASE     = 64;
static const int PORT_H          = 20;
static const int NODE_H_PER_PORT = PORT_H;
static const int COL_GAP         = 120;
static const int ROW_GAP         = 40;
static const int MARGIN          = 30;
static const int PORT_W          = 8;
static const int RADIUS          = 8;

// ============================================================================
// TypeFlowCanvas
// ============================================================================

TypeFlowCanvas::TypeFlowCanvas(QWidget* parent)
    : QWidget(parent)
{
    // Initialize with dark (Catppuccin Mocha) colors via Theme::Css constants
    m_isDark        = true;
    m_bgColor       = QColor(BG);
    m_nodeColor     = QColor("#313244");   // surface0, no Css constant
    m_nodeBorder    = QColor("#585b70");   // overlay0, no Css constant
    m_nodeTextColor = QColor(FG);
    m_typeColor     = QColor(ACCENT);
    m_arrowColor    = QColor(FG2);
    m_errorColor    = QColor(RED);
    m_portBg        = QColor(BORDER);
    setMinimumSize(400, 300);
}


void TypeFlowCanvas::setNodes(const QVector<FuncNode>& nodes, const QVector<FlowEdge>& edges)
{
    m_nodes = nodes;
    m_edges = edges;
    layoutNodes();
    updateGeometry();
    update();
}

void TypeFlowCanvas::layoutNodes()
{
    // Simple left-to-right column layout.
    // Nodes that feed into others go left; nodes with no inputs go leftmost.
    // For simplicity: lay out in order, one column per function.
    // If there are many nodes, wrap into multiple rows.

    const int maxPerCol = 6;
    int x = MARGIN;
    int y = MARGIN;
    int col = 0;

    for (int i = 0; i < m_nodes.size(); ++i) {
        int numPorts = qMax(1, m_nodes[i].inputs.size());
        int h = NODE_H_BASE + (numPorts - 1) * NODE_H_PER_PORT;
        m_nodes[i].rect = QRect(x, y, NODE_W, h);

        y += h + ROW_GAP;
        if ((i + 1) % maxPerCol == 0) {
            col++;
            x += NODE_W + COL_GAP;
            y = MARGIN;
        }
    }
}

QSize TypeFlowCanvas::sizeHint() const
{
    if (m_nodes.isEmpty()) return QSize(400, 300);
    int maxX = 0, maxY = 0;
    for (const auto& n : m_nodes) {
        maxX = qMax(maxX, n.rect.right());
        maxY = qMax(maxY, n.rect.bottom());
    }
    return QSize(maxX + MARGIN + COL_GAP, maxY + MARGIN * 2);
}

QPoint TypeFlowCanvas::leftPortPos(const FuncNode& node, int portIndex) const
{
    int y = node.rect.top() + NODE_H_BASE / 2 + portIndex * NODE_H_PER_PORT;
    return QPoint(node.rect.left(), y);
}

QPoint TypeFlowCanvas::rightPortPos(const FuncNode& node) const
{
    return QPoint(node.rect.right(), node.rect.top() + NODE_H_BASE / 2);
}

void TypeFlowCanvas::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), m_bgColor);

    // Draw edges first (behind nodes)
    for (const auto& edge : m_edges) {
        drawEdge(p, edge);
    }

    // Draw nodes
    for (const auto& node : m_nodes) {
        drawNode(p, node);
    }
}

void TypeFlowCanvas::drawNode(QPainter& p, const FuncNode& node)
{
    QColor border = node.hasError ? m_errorColor : m_nodeBorder;

    // Node body
    p.setPen(QPen(border, node.hasError ? 2 : 1));
    p.setBrush(m_nodeColor);
    p.drawRoundedRect(node.rect, RADIUS, RADIUS);

    // Function name
    QFont nameFont = p.font();
    nameFont.setBold(true);
    nameFont.setPointSize(10);
    p.setFont(nameFont);
    p.setPen(m_nodeTextColor);

    QRect nameRect(node.rect.left() + 8, node.rect.top() + 8,
                   node.rect.width() - 16, 20);
    p.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, node.funcName + "()");

    // Divider
    p.setPen(QPen(m_nodeBorder, 1));
    p.drawLine(node.rect.left() + 4, node.rect.top() + 28,
               node.rect.right() - 4, node.rect.top() + 28);

    // Input ports (left side)
    QFont portFont = p.font();
    portFont.setBold(false);
    portFont.setPointSize(8);
    p.setFont(portFont);

    for (int i = 0; i < node.inputs.size(); ++i) {
        QPoint portCenter = leftPortPos(node, i);
        const TypePort& port = node.inputs[i];

        QColor portColor = port.isError ? m_errorColor : m_typeColor;

        // Port dot
        p.setPen(Qt::NoPen);
        p.setBrush(portColor);
        p.drawEllipse(portCenter + QPoint(-PORT_W/2, -PORT_W/2), PORT_W, PORT_W);

        // Label: "name: type"
        QString lbl = port.name.isEmpty()
                      ? port.typeName
                      : port.name + ": " + port.typeName;
        p.setPen(portColor);
        QRect lblRect(portCenter.x() + PORT_W, portCenter.y() - 9,
                      node.rect.width() / 2 - PORT_W - 4, 18);
        p.drawText(lblRect, Qt::AlignLeft | Qt::AlignVCenter, lbl);
    }

    // Output port (right side)
    {
        QPoint portCenter = rightPortPos(node);
        QColor portColor = node.output.isError ? m_errorColor : m_typeColor;

        p.setPen(Qt::NoPen);
        p.setBrush(portColor);
        p.drawEllipse(portCenter + QPoint(-PORT_W/2, -PORT_W/2), PORT_W, PORT_W);

        // Return type label (right-aligned, inside node)
        QString retLabel = "-> " + node.output.typeName;
        p.setPen(portColor);
        QRect retRect(node.rect.right() - 80, portCenter.y() - 9, 72, 18);
        p.drawText(retRect, Qt::AlignRight | Qt::AlignVCenter, retLabel);
    }
}

void TypeFlowCanvas::drawEdge(QPainter& p, const FlowEdge& edge)
{
    if (edge.fromFunc < 0 || edge.fromFunc >= m_nodes.size()) return;
    if (edge.toFunc   < 0 || edge.toFunc   >= m_nodes.size()) return;

    QPoint from = rightPortPos(m_nodes[edge.fromFunc]);
    QPoint to   = leftPortPos(m_nodes[edge.toFunc], edge.toPortIndex);

    QColor arrowColor = edge.isError ? m_errorColor : m_arrowColor;
    p.setPen(QPen(arrowColor, 2));

    // Bezier curve
    QPainterPath path;
    path.moveTo(from);
    int cx = (from.x() + to.x()) / 2;
    path.cubicTo(QPoint(cx, from.y()), QPoint(cx, to.y()), to);
    p.drawPath(path);

    // Arrowhead at 'to'
    p.setBrush(arrowColor);
    p.setPen(Qt::NoPen);
    QPolygon arrow;
    arrow << QPoint(to.x() - 8, to.y() - 4)
          << QPoint(to.x(),     to.y())
          << QPoint(to.x() - 8, to.y() + 4);
    p.drawPolygon(arrow);

    // Type label in middle of edge
    QPoint mid((from.x() + to.x()) / 2, (from.y() + to.y()) / 2 - 10);
    p.setPen(arrowColor);
    QFont f = p.font();
    f.setPointSize(8);
    p.setFont(f);
    p.drawText(QRect(mid.x() - 40, mid.y() - 8, 80, 16),
               Qt::AlignCenter, edge.fromType);
}

void TypeFlowCanvas::mousePressEvent(QMouseEvent* event)
{
    for (const auto& node : m_nodes) {
        if (node.rect.contains(event->pos())) {
            emit funcClicked(node.sourceLine);
            return;
        }
    }
}

// ============================================================================
// Code Analysis
// ============================================================================

// Heuristic Python type inference from an expression
QString TypeFlowFrame::inferPythonType(const QString& expr)
{
    QString e = expr.trimmed();
    if (e.isEmpty()) return "None";

    // Integer literal
    static QRegularExpression reInt(R"(^-?\d+$)");
    if (reInt.match(e).hasMatch()) return "int";

    // Float literal
    static QRegularExpression reFloat(R"(^-?\d+\.\d*)");
    if (reFloat.match(e).hasMatch()) return "float";

    // String literal
    if ((e.startsWith('"') && e.endsWith('"')) ||
        (e.startsWith('\'') && e.endsWith('\''))) return "str";
    if (e.startsWith("f\"") || e.startsWith("f'")) return "str";

    // Bool
    if (e == "True" || e == "False") return "bool";

    // None
    if (e == "None") return "None";

    // List literal
    if (e.startsWith('[')) return "list";

    // Dict literal
    if (e.startsWith('{') && e.contains(':')) return "dict";

    // Set literal
    if (e.startsWith('{')) return "set";

    // Tuple literal
    if (e.startsWith('(') && e.contains(',')) return "tuple";

    // Common constructors
    static const QMap<QString, QString> ctors = {
        {"int(", "int"}, {"float(", "float"}, {"str(", "str"},
        {"list(", "list"}, {"dict(", "dict"}, {"set(", "set"},
        {"tuple(", "tuple"}, {"bool(", "bool"}, {"bytes(", "bytes"},
        {"bytearray(", "bytearray"}, {"len(", "int"}, {"range(", "range"},
    };
    for (auto it = ctors.begin(); it != ctors.end(); ++it) {
        if (e.startsWith(it.key())) return it.value();
    }

    // String methods that return str
    static QRegularExpression reStrMethod(R"(\.(join|strip|replace|upper|lower|format)\()");
    if (reStrMethod.match(e).hasMatch()) return "str";

    // Arithmetic: if contains +/-/*/  and no quotes, assume numeric
    if (!e.contains('"') && !e.contains('\'')) {
        if (e.contains('+') || e.contains('-') || e.contains('*') || e.contains('/'))
            return "int";
    }

    return "Any";
}

// Parse a simple C type string
QString TypeFlowFrame::parseCType(const QString& tokens)
{
    QString t = tokens.trimmed();
    // Remove pointer stars from end
    while (t.endsWith('*')) t.chop(1);
    t = t.trimmed();
    // Normalize common types
    if (t.isEmpty()) return "void";
    return t;
}

// Analyze Python code: find functions, infer param/return types
QVector<FuncNode> TypeFlowFrame::analyzePython(const QStringList& lines)
{
    QVector<FuncNode> nodes;

    // Variable type inference: track assignments at module level
    QMap<QString, QString> varTypes;

    // First pass: collect variable assignments
    static QRegularExpression reAssign(R"(^(\s*)([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+)$)");
    for (const QString& line : lines) {
        auto m = reAssign.match(line);
        if (m.hasMatch() && !line.trimmed().startsWith("def ")) {
            QString indent = m.captured(1);
            if (indent.isEmpty()) { // module-level
                QString name  = m.captured(2);
                QString value = m.captured(3).trimmed();
                varTypes[name] = inferPythonType(value);
            }
        }
    }

    // Second pass: find function definitions
    static QRegularExpression reDef(R"(^def\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)\s*(?:->\s*([^:]+))?\s*:)");
    // Also handle async def
    static QRegularExpression reAsyncDef(R"(^async\s+def\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)\s*(?:->\s*([^:]+))?\s*:)");

    for (int lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
        const QString& line = lines[lineIdx];
        QString trimmed = line.trimmed();

        QRegularExpressionMatch m;
        bool matched = false;
        if ((m = reDef.match(trimmed)).hasMatch()) matched = true;
        else if ((m = reAsyncDef.match(trimmed)).hasMatch()) matched = true;

        if (!matched) continue;

        FuncNode node;
        node.funcName   = m.captured(1);
        node.sourceLine = lineIdx + 1;
        node.hasError   = false;

        // Parse parameters
        QString paramStr = m.captured(2).trimmed();
        if (!paramStr.isEmpty()) {
            const QStringList params = paramStr.split(',');
            for (const QString& param : params) {
                QString p = param.trimmed();
                if (p.isEmpty() || p == "self" || p == "cls") continue;

                TypePort port;
                // Check for type annotation: name: type
                if (p.contains(':')) {
                    int colon = p.indexOf(':');
                    port.name     = p.left(colon).trimmed();
                    QString ann   = p.mid(colon + 1).trimmed();
                    // Remove default value
                    if (ann.contains('=')) ann = ann.left(ann.indexOf('=')).trimmed();
                    port.typeName = ann;
                } else {
                    // Remove default value
                    QString pname = p;
                    if (pname.contains('=')) pname = pname.left(pname.indexOf('=')).trimmed();
                    // Remove * or **
                    pname = pname.remove('*').trimmed();
                    port.name = pname;
                    // Guess type from variable table
                    port.typeName = varTypes.value(pname, "Any");
                }
                port.isError = false;
                node.inputs.append(port);
            }
        }

        // Return type annotation
        if (m.lastCapturedIndex() >= 3 && !m.captured(3).isEmpty()) {
            node.output.typeName = m.captured(3).trimmed();
        } else {
            // Scan body for return statements to infer type
            QString retType = "None";
            for (int j = lineIdx + 1; j < lines.size() && j < lineIdx + 40; ++j) {
                QString bodyLine = lines[j].trimmed();
                if (!bodyLine.startsWith("def ") && bodyLine.startsWith("return ")) {
                    QString retExpr = bodyLine.mid(7).trimmed();
                    if (!retExpr.isEmpty() && retExpr != "None") {
                        retType = inferPythonType(retExpr);
                    }
                    break;
                }
            }
            node.output.typeName = retType;
        }
        node.output.name    = "";
        node.output.isError = false;

        nodes.append(node);
    }

    return nodes;
}

// Analyze C/UL code: find functions with explicit types
QVector<FuncNode> TypeFlowFrame::analyzeC(const QStringList& lines)
{
    QVector<FuncNode> nodes;

    // Pattern: returnType funcName(params) {
    static QRegularExpression reFuncC(
        R"(^([A-Za-z_][A-Za-z0-9_\s\*]*?)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)\s*\{?)");

    // Keywords that are not function return types
    static const QSet<QString> notTypes = {
        "if","else","while","for","do","switch","return","break","continue","goto",
        "struct","union","enum","typedef","class","namespace","template","public",
        "private","protected","static","extern","inline","virtual","override"
    };

    for (int lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
        const QString& line = lines[lineIdx];
        QString trimmed = line.trimmed();

        // Skip preprocessor, comments, blank
        if (trimmed.startsWith('#') || trimmed.startsWith("//") ||
            trimmed.startsWith("/*") || trimmed.isEmpty()) continue;

        auto m = reFuncC.match(trimmed);
        if (!m.hasMatch()) continue;

        QString retType  = m.captured(1).trimmed();
        QString funcName = m.captured(2).trimmed();
        QString paramStr = m.captured(3).trimmed();

        // Check return type is not a keyword
        QString rtCheck = retType.split(QRegularExpression("\\s+")).last().remove('*').trimmed();
        if (notTypes.contains(rtCheck)) continue;
        if (retType.isEmpty()) continue;

        FuncNode node;
        node.funcName   = funcName;
        node.sourceLine = lineIdx + 1;
        node.hasError   = false;

        // Parse parameters
        if (!paramStr.isEmpty() && paramStr != "void") {
            const QStringList params = paramStr.split(',');
            for (const QString& param : params) {
                QString p = param.trimmed();
                if (p.isEmpty()) continue;

                TypePort port;
                // Last token is the name, everything before is the type
                QStringList tokens = p.split(QRegularExpression("\\s+"));
                if (tokens.size() == 1) {
                    port.typeName = tokens[0];
                    port.name     = "";
                } else {
                    port.name     = tokens.last().remove('*').trimmed();
                    tokens.removeLast();
                    port.typeName = parseCType(tokens.join(" "));
                }
                port.isError = false;
                node.inputs.append(port);
            }
        }

        node.output.typeName = parseCType(retType);
        node.output.name     = "";
        node.output.isError  = false;

        nodes.append(node);
    }

    return nodes;
}

QVector<FuncNode> TypeFlowFrame::analyzeCode(const QString& code, const QString& lang)
{
    QStringList lines = code.split('\n');
    if (lang == "python") return analyzePython(lines);
    return analyzeC(lines); // covers C and UL
}

// Build edges: detect where a function's output feeds into another's input
// by looking for call patterns: funcName(varName) and matching variable types
QVector<FlowEdge> TypeFlowFrame::buildEdges(const QVector<FuncNode>& nodes,
                                             const QStringList& lines,
                                             const QString& lang)
{
    QVector<FlowEdge> edges;
    Q_UNUSED(lang);

    // Build a map: funcName -> node index
    QMap<QString, int> nameToIdx;
    for (int i = 0; i < nodes.size(); ++i) {
        nameToIdx[nodes[i].funcName] = i;
    }

    // For each pair of nodes, check if one calls the other
    // Scan the source lines for calls of the form: funcName(...)
    for (int callerIdx = 0; callerIdx < nodes.size(); ++callerIdx) {
        const FuncNode& caller = nodes[callerIdx];

        // Find the body of this function (lines after its definition)
        int startLine = caller.sourceLine; // 1-based
        int endLine   = lines.size();
        // Find next top-level function definition
        for (int ni = 0; ni < nodes.size(); ++ni) {
            if (ni != callerIdx && nodes[ni].sourceLine > startLine) {
                endLine = qMin(endLine, nodes[ni].sourceLine - 1);
            }
        }

        // Scan body lines for calls to other known functions
        for (int li = startLine; li < endLine && li < lines.size(); ++li) {
            const QString& bodyLine = lines[li].trimmed();

            for (int calleeIdx = 0; calleeIdx < nodes.size(); ++calleeIdx) {
                if (calleeIdx == callerIdx) continue;
                const FuncNode& callee = nodes[calleeIdx];

                QString callPat = callee.funcName + "(";
                if (!bodyLine.contains(callPat)) continue;

                // Found a call: caller's output feeds into callee's call site
                // Check if caller's return value matches callee's first input
                FlowEdge edge;
                edge.fromFunc    = callerIdx;
                edge.toFunc      = calleeIdx;
                edge.toPortIndex = 0;
                edge.fromType    = caller.output.typeName;
                edge.toType      = callee.inputs.isEmpty()
                                   ? "" : callee.inputs[0].typeName;

                // Type error if types differ and neither is Any/void
                bool fromAny = (edge.fromType == "Any" || edge.fromType == "void"
                                || edge.fromType.isEmpty());
                bool toAny   = (edge.toType   == "Any" || edge.toType.isEmpty());
                edge.isError = !fromAny && !toAny && (edge.fromType != edge.toType);

                edges.append(edge);
                break; // one edge per callee per caller pass
            }
        }
    }

    return edges;
}

// ============================================================================
// TypeFlowFrame
// ============================================================================

TypeFlowFrame::TypeFlowFrame(QWidget* parent)
    : AnalysisFrame("Type Flow", parent)
{
    // ── Legend bar ────────────────────────────────────────────────────────────
    auto* legendBar = new QWidget;
    legendBar->setFixedHeight(28);
    legendBar->setStyleSheet(QString("background: %1; border-bottom: 1px solid #313244;").arg(BG));
    auto* legendLayout = new QHBoxLayout(legendBar);
    legendLayout->setContentsMargins(16, 0, 16, 0);
    legendLayout->setSpacing(18);

    auto makeLegend = [&](const QString& color, const QString& text) {
        auto* row = new QHBoxLayout;
        row->setSpacing(5);
        auto* dot = new QLabel;
        dot->setFixedSize(10, 10);
        dot->setStyleSheet(QString("background: %1; border-radius: 5px;").arg(color));
        auto* lbl = new QLabel(text);
        lbl->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent;").arg(FG2));
        row->addWidget(dot);
        row->addWidget(lbl);
        legendLayout->addLayout(row);
    };

    makeLegend(ACCENT, "Type port");
    makeLegend(RED,    "Type error");
    makeLegend(FG2,    "Data flow arrow");
    legendLayout->addStretch();
    auto* hint = new QLabel("Click any function box to jump to it in the editor");
    hint->setStyleSheet("color: #585b70; font-size: 11px; background: transparent;");  // overlay0, no Css constant
    legendLayout->addWidget(hint);

    // Run button control bar
    m_runBtn = new QPushButton("Run Analysis");
    m_runBtn->setFixedHeight(28);
    m_runBtn->setCursor(Qt::PointingHandCursor);
    m_runBtn->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: none;"
        " border-radius: 4px; padding: 0 14px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background: %3; }").arg(ACCENT, BG, ACCENT2));

    // ── Scrollable canvas ─────────────────────────────────────────────────────
    m_canvas = new TypeFlowCanvas;
    m_scroll = new QScrollArea;
    m_scroll->setWidget(m_canvas);
    m_scroll->setWidgetResizable(false);
    m_scroll->setStyleSheet(QString("QScrollArea { background: %1; border: none; }").arg(BG));

    // Insert controls and canvas into inherited results area
    addResultWidget(legendBar);
    addResultWidget(m_runBtn);
    addResultWidget(m_scroll);

    // ── Status label ──────────────────────────────────────────────────────────
    setStatus("Click 'Run Analysis' to scan your code.");

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_runBtn,  &QPushButton::clicked, this, &TypeFlowFrame::onRunAnalysis);
    connect(m_canvas,  &TypeFlowCanvas::funcClicked, this, &TypeFlowFrame::onFuncClicked);
}

void TypeFlowFrame::setCode(const QString& code, const QString& language)
{
    AnalysisFrame::setCode(code, language);
    setStatus("Click 'Run Analysis' to scan your code.");
}


void TypeFlowFrame::onRunAnalysis()
{
    if (m_code.trimmed().isEmpty()) {
        m_statusLabel->setText("No code to analyze — open a file first.");
        return;
    }

    m_statusLabel->setText("Analyzing...");

    QStringList lines = m_code.split('\n');
    QVector<FuncNode> nodes = analyzeCode(m_code, m_language);
    QVector<FlowEdge> edges = buildEdges(nodes, lines, m_language);

    if (nodes.isEmpty()) {
        m_statusLabel->setText("No functions found in this file.");
        m_canvas->setNodes({}, {});
        m_canvas->update();
        return;
    }

    // Count errors
    int errCount = 0;
    for (const auto& e : edges) if (e.isError) errCount++;

    m_statusLabel->setText(
        QString("%1 function%2 found.%3")
            .arg(nodes.size())
            .arg(nodes.size() == 1 ? "" : "s")
            .arg(errCount > 0
                 ? QString("  %1 type error%2 detected.").arg(errCount).arg(errCount == 1 ? "" : "s")
                 : "  No type errors detected.")
    );

    m_canvas->setNodes(nodes, edges);
    m_canvas->update();
}

void TypeFlowFrame::onFuncClicked(int sourceLine)
{
    emit jumpToLine(QString(), sourceLine);
    emit backToEditor();
}
