#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVector>
#include <QString>
#include <QRect>
#include <QPoint>
#include <QColor>

// ============================================================================
// TypeFlowVisualizer — "Type Flow" feature
//
// Shows how types flow through code like a circuit diagram:
//   - Each function is a rounded rectangle box
//   - Input types (parameters) appear on the left side
//   - Output type (return) appears on the right side
//   - Arrows connect outputs to where they're used as inputs
//   - Type errors highlight the broken connection red
//
// Supports Python (heuristic type inference) and C/UL (explicit declarations).
// Clicking a function box highlights that function in the editor.
// ============================================================================

// ── One parameter or return type port ────────────────────────────────────────
struct TypePort {
    QString name;       // variable/param name
    QString typeName;   // inferred or declared type (e.g. "int", "str", "list")
    bool    isError;    // true if type mismatch detected at this port
};

// ── One function node in the flow graph ──────────────────────────────────────
struct FuncNode {
    QString            funcName;
    QVector<TypePort>  inputs;       // parameters (left side)
    TypePort           output;       // return type (right side)
    int                sourceLine;   // 1-based line number in editor
    QRect              rect;         // layout position (set during layout)
    bool               hasError;     // any port has a type error
};

// ── A directed edge connecting one function's output to another's input ───────
struct FlowEdge {
    int  fromFunc;      // index into m_nodes
    int  toFunc;        // index into m_nodes
    int  toPortIndex;   // which input port on toFunc
    bool isError;       // mismatched types
    QString fromType;
    QString toType;
};

// ── The canvas widget that draws all nodes + edges ───────────────────────────
class TypeFlowCanvas : public QWidget {
    Q_OBJECT
public:
    explicit TypeFlowCanvas(QWidget* parent = nullptr);

    void setNodes(const QVector<FuncNode>& nodes, const QVector<FlowEdge>& edges);
    void applyTheme(bool isDark);
    QSize sizeHint() const override;

signals:
    void funcClicked(int sourceLine);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void layoutNodes();
    void drawNode(QPainter& p, const FuncNode& node);
    void drawEdge(QPainter& p, const FlowEdge& edge);
    QPoint leftPortPos(const FuncNode& node, int portIndex) const;
    QPoint rightPortPos(const FuncNode& node) const;

    QVector<FuncNode> m_nodes;
    QVector<FlowEdge> m_edges;
    bool m_isDark = true;

    // Colors
    QColor m_bgColor;
    QColor m_nodeColor;
    QColor m_nodeBorder;
    QColor m_nodeTextColor;
    QColor m_typeColor;
    QColor m_arrowColor;
    QColor m_errorColor;
    QColor m_portBg;
};

// ── The full Type Flow frame (header + scrollable canvas) ─────────────────────
class TypeFlowFrame : public QWidget {
    Q_OBJECT
public:
    explicit TypeFlowFrame(QWidget* parent = nullptr);

    // Provide code + language before showing; triggers analysis
    void setCode(const QString& code, const QString& language);
    void applyTheme(bool isDark);

signals:
    void backToEditor();
    void jumpToLine(int line);   // emitted when user clicks a function box

private slots:
    void onRunAnalysis();
    void onFuncClicked(int sourceLine);

private:
    // ── Analysis ──────────────────────────────────────────────────────────────
    static QVector<FuncNode> analyzeCode(const QString& code, const QString& lang);
    static QVector<FuncNode> analyzePython(const QStringList& lines);
    static QVector<FuncNode> analyzeC(const QStringList& lines);
    static QVector<FlowEdge> buildEdges(const QVector<FuncNode>& nodes,
                                         const QStringList& lines,
                                         const QString& lang);
    static QString inferPythonType(const QString& valueExpr);
    static QString parseCType(const QString& tokens);

    // ── Widgets ───────────────────────────────────────────────────────────────
    QPushButton*     m_backBtn      = nullptr;
    QPushButton*     m_runBtn       = nullptr;
    QLabel*          m_statusLabel  = nullptr;
    QScrollArea*     m_scroll       = nullptr;
    TypeFlowCanvas*  m_canvas       = nullptr;

    // ── State ─────────────────────────────────────────────────────────────────
    QString  m_code;
    QString  m_language;
    bool     m_isDark = true;
};
