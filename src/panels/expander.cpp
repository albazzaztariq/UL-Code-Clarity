#include "panels/expander.h"
#include <QKeyEvent>
#include <QRegularExpression>
#include <QStringList>

// ============================================================================
// Theme colors
// ============================================================================
static const char* BG3     = "#333348";
static const char* BG      = "#1e1e2e";
static const char* BG4     = "#3c3c54";
static const char* FG      = "#cdd6f4";
static const char* FG2     = "#a6adc8";
static const char* FG3     = "#6c7086";
static const char* ACCENT  = "#89b4fa";
static const char* GREEN   = "#a6e3a1";
static const char* PEACH   = "#fab387";
static const char* BORDER  = "#45475a";

// ============================================================================
// ExpanderOverlay
// ============================================================================

ExpanderOverlay::ExpanderOverlay(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(560);
    setStyleSheet(QString(
        "ExpanderOverlay { background: %1; border: 1px solid %2; border-radius: 6px; }"
    ).arg(BG3, BORDER));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(12);

    m_titleLabel = new QLabel("Statement Expander", this);
    m_titleLabel->setStyleSheet(QString(
        "font-size: 13px; font-weight: 700; color: %1;"
    ).arg(ACCENT));
    mainLayout->addWidget(m_titleLabel);

    m_descLabel = new QLabel(
        "Breaks compressed/nested statements into one operation per line.", this);
    m_descLabel->setStyleSheet(QString("font-size: 11px; color: %1;").arg(FG2));
    m_descLabel->setWordWrap(true);
    mainLayout->addWidget(m_descLabel);

    // Demo frame
    m_demoFrame = new QFrame(this);
    m_demoFrame->setStyleSheet(QString(
        "QFrame { background: %1; border-radius: 6px; padding: 12px; }"
    ).arg(BG));

    auto *demoLayout = new QVBoxLayout(m_demoFrame);
    demoLayout->setContentsMargins(12, 12, 12, 12);
    demoLayout->setSpacing(8);

    m_originalLabel = new QLabel(m_demoFrame);
    m_originalLabel->setStyleSheet(QString(
        "font-family: 'Cascadia Code', 'Fira Code', 'Consolas', monospace; "
        "font-size: 11px; color: %1;"
    ).arg(FG3));
    m_originalLabel->setWordWrap(true);
    demoLayout->addWidget(m_originalLabel);

    m_arrowLabel = new QLabel(m_demoFrame);
    m_arrowLabel->setText(QString::fromUtf8("\xe2\x86\x93")); // down arrow
    m_arrowLabel->setAlignment(Qt::AlignCenter);
    m_arrowLabel->setStyleSheet(QString("font-size: 18px; color: %1;").arg(FG3));
    demoLayout->addWidget(m_arrowLabel);

    m_expandedLabel = new QLabel(m_demoFrame);
    m_expandedLabel->setStyleSheet(QString(
        "font-family: 'Cascadia Code', 'Fira Code', 'Consolas', monospace; "
        "font-size: 11px; color: %1;"
    ).arg(GREEN));
    m_expandedLabel->setWordWrap(true);
    demoLayout->addWidget(m_expandedLabel);

    mainLayout->addWidget(m_demoFrame);

    // Buttons row
    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();

    m_expandButton = new QPushButton("Expand Current File", this);
    m_expandButton->setStyleSheet(QString(
        "QPushButton { background: %1; color: #1e1e2e; font-weight: 600; "
        "padding: 5px 14px; border-radius: 6px; font-size: 11px; }"
        "QPushButton:hover { filter: brightness(1.15); }"
    ).arg(PEACH));
    connect(m_expandButton, &QPushButton::clicked,
            this, &ExpanderOverlay::expandFileRequested);
    buttonRow->addWidget(m_expandButton);

    m_closeButton = new QPushButton("Close", this);
    m_closeButton->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; padding: 5px 14px; "
        "border-radius: 6px; font-size: 11px; }"
    ).arg(BG4, FG2));
    connect(m_closeButton, &QPushButton::clicked,
            this, &ExpanderOverlay::closeRequested);
    buttonRow->addWidget(m_closeButton);

    mainLayout->addLayout(buttonRow);

    setVisible(false);
}

void ExpanderOverlay::showWithDemo(const QString &original, const QString &expanded)
{
    m_originalLabel->setText(original);

    // Format expanded with markers
    QString formatted = QString(
        "<span style='color:%1; font-style:italic'># -- Expanded by Clarity --</span><br>"
        "%2<br>"
        "<span style='color:%1; font-style:italic'># -- End expansion --</span>"
    ).arg(FG3, expanded);
    m_expandedLabel->setTextFormat(Qt::RichText);
    m_expandedLabel->setText(formatted);

    setVisible(true);
    raise();
}

void ExpanderOverlay::showDefaultDemo()
{
    showWithDemo(
        "result = transform(normalize(data.get_values()), config.settings[\"threshold\"] * scale)",
        "values = data.get_values()<br>"
        "normalized = normalize(values)<br>"
        "threshold = config.settings[\"threshold\"]<br>"
        "scaled = threshold * scale<br>"
        "result = transform(normalized, scaled)"
    );
}

void ExpanderOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit closeRequested();
    }
    QWidget::keyPressEvent(event);
}

// ============================================================================
// WalkThroughHandler
// ============================================================================

WalkThroughHandler::WalkThroughHandler(QObject *parent)
    : QObject(parent)
{
}

// ─── Local code analysis helpers ─────────────────────────────────────────────

struct CodeAnalysis {
    QStringList functions;      // function/method names found
    QStringList classes;        // class/struct names found
    QStringList imports;        // import/include statements
    QStringList loops;          // for/while keywords with context
    QStringList conditionals;   // if/else/switch
    QStringList returns;        // return statements
    QString     entryPoint;     // main() or if __name__ == '__main__'
    QString     lang;           // detected language hint
    int         lineCount = 0;
    int         varCount = 0;
};

static CodeAnalysis analyzeCode(const QString &code, const QString &filename)
{
    CodeAnalysis a;
    QStringList lines = code.split('\n');
    a.lineCount = lines.size();

    // Detect language from filename extension
    QString ext;
    int dot = filename.lastIndexOf('.');
    if (dot >= 0) ext = filename.mid(dot + 1).toLower();
    if (ext == "py")                              a.lang = "python";
    else if (ext == "c" || ext == "h")            a.lang = "c";
    else if (ext == "cpp" || ext == "cxx" || ext == "cc" || ext == "hpp") a.lang = "cpp";
    else if (ext == "rs")                         a.lang = "rust";
    else if (ext == "ul")                         a.lang = "ul";
    else if (ext == "js" || ext == "ts")          a.lang = "js";
    else                                          a.lang = "text";

    // Regex patterns
    // Python: def funcname(
    QRegularExpression pyFunc(R"(^\s*def\s+(\w+)\s*\()");
    // Python: class Name
    QRegularExpression pyClass(R"(^\s*class\s+(\w+))");
    // C/C++/Rust/UL: return type funcname(  — e.g. int main(, void foo(, fn bar(
    QRegularExpression cFunc(R"(^\s*(?:(?:static|inline|extern|public|private|protected|virtual|override|fn|func)\s+)*(?:\w[\w:<>*&\s]*\s+)?(\w+)\s*\([^;]*$)");
    // C struct/class
    QRegularExpression cClass(R"(^\s*(?:struct|class|enum)\s+(\w+))");
    // Python import
    QRegularExpression pyImport(R"(^\s*(?:import|from)\s+([\w\.]+))");
    // C #include
    QRegularExpression cInclude(R"(^\s*#include\s*[<"]([\w./]+)[>"]))");
    // Loop keywords
    QRegularExpression loopRx(R"(^\s*(for|while|foreach)\b)");
    // Conditionals
    QRegularExpression condRx(R"(^\s*(if|elif|else|switch|case)\b)");
    // Return
    QRegularExpression retRx(R"(^\s*return\b)");
    // Variable assignment (simple: identifier = )
    QRegularExpression varRx(R"(^\s*(?:let|var|const|auto)?\s*\w+\s*=\s*\S)");
    // Python main guard
    QRegularExpression pyMain(R"(if\s+__name__\s*==\s*['"']__main__['"'])");

    // Rust fn main / C int main
    QRegularExpression mainFn(R"(\bmain\s*\()");

    for (const QString &rawLine : lines) {
        QString line = rawLine;

        // Skip comments and blank lines
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;
        if (trimmed.startsWith("//") || trimmed.startsWith("#!") ||
            (trimmed.startsWith("#") && a.lang != "python" && a.lang != "ul")) continue;
        // For Python/UL: allow # comments to be skipped
        if ((a.lang == "python" || a.lang == "ul") && trimmed.startsWith("#")) continue;

        // Entry point
        if (pyMain.match(line).hasMatch()) a.entryPoint = "__main__ guard";
        if (mainFn.match(line).hasMatch() && a.entryPoint.isEmpty()) a.entryPoint = "main()";

        // Functions
        if (a.lang == "python" || a.lang == "ul") {
            auto m = pyFunc.match(line);
            if (m.hasMatch()) {
                QString name = m.captured(1);
                if (!a.functions.contains(name)) a.functions << name;
            }
            auto mc = pyClass.match(line);
            if (mc.hasMatch()) {
                QString name = mc.captured(1);
                if (!a.classes.contains(name)) a.classes << name;
            }
            auto mi = pyImport.match(line);
            if (mi.hasMatch()) {
                QString name = mi.captured(1);
                if (!a.imports.contains(name)) a.imports << name;
            }
        } else {
            // C/C++/Rust/JS — function detection: line ends without semicolon and has paren
            // Use a simpler heuristic: look for word( pattern at start of line (not keyword)
            static QRegularExpression simpleFn(R"(^\s*(?:(?:static|inline|extern|pub|fn|func|async)\s+)*(?:[\w:*&<>\[\]]+\s+)+(\w+)\s*\()");
            auto m = simpleFn.match(line);
            if (m.hasMatch()) {
                QString name = m.captured(1);
                static QStringList kw = {"if","for","while","switch","return","else",
                                         "do","case","sizeof","alignof","typeof"};
                if (!kw.contains(name) && !a.functions.contains(name))
                    a.functions << name;
            }
            auto mc = cClass.match(line);
            if (mc.hasMatch()) {
                QString name = mc.captured(1);
                if (!a.classes.contains(name)) a.classes << name;
            }
            auto mi = cInclude.match(line);
            if (mi.hasMatch()) {
                QString name = mi.captured(1);
                if (!a.imports.contains(name)) a.imports << name;
            }
        }

        // Loops, conditionals, returns
        if (loopRx.match(line).hasMatch()) {
            // Capture first few words for context
            QString ctx = trimmed.left(50);
            if (a.loops.size() < 6) a.loops << ctx;
        }
        if (condRx.match(line).hasMatch()) {
            if (a.conditionals.size() < 6) a.conditionals << trimmed.left(50);
        }
        if (retRx.match(line).hasMatch()) {
            if (a.returns.size() < 4) a.returns << trimmed.left(60);
        }
        if (varRx.match(line).hasMatch()) {
            a.varCount++;
        }
    }

    return a;
}

// Build call-graph string: "main() → foo() → bar()"
static QString callGraph(const QStringList &fns) {
    if (fns.isEmpty()) return "(no functions detected)";
    // Heuristic: first function is entry, rest are callees
    QStringList wrapped;
    for (const QString &f : fns) wrapped << "<code>" + f + "()</code>";
    if (wrapped.size() == 1) return wrapped.first();
    return wrapped.first() + " &rarr; " + wrapped.mid(1).join(", ");
}

QString WalkThroughHandler::generateWalkthrough(const QString &filename,
                                                 const QString &code,
                                                 int level) const
{
    if (code.trimmed().isEmpty()) {
        return QString("<b>Walk Me Through:</b> The editor is empty. Open a file first.");
    }

    CodeAnalysis a = analyzeCode(code, filename);

    int fnCount    = a.functions.size();
    int classCount = a.classes.size();
    int loopCount  = a.loops.size();
    int condCount  = a.conditionals.size();
    int importCount= a.imports.size();

    QString result;

    // ── Level 1 — Beginner ────────────────────────────────────────────────
    if (level == 1) {
        result += QString("<b>Walking you through <code>%1</code>:</b><br><br>").arg(filename);

        // Summary sentence
        QStringList parts;
        if (fnCount > 0)    parts << QString("%1 function%2").arg(fnCount).arg(fnCount > 1 ? "s" : "");
        if (classCount > 0) parts << QString("%1 class%2").arg(classCount).arg(classCount > 1 ? "es" : "");
        if (importCount > 0) parts << QString("%1 import%2").arg(importCount).arg(importCount > 1 ? "s" : "");
        result += "<b>Overview:</b> This program has " + (parts.isEmpty() ? "no named sections" : parts.join(", ")) + ".<br><br>";

        // Functions explained
        if (!a.functions.isEmpty()) {
            result += "<b>What each function does:</b><br>";
            for (int i = 0; i < a.functions.size(); ++i) {
                QString fn = a.functions[i];
                if (fn == "main") {
                    result += QString("&bull; <code>%1()</code> — this is where the program <b>starts</b>. "
                                      "Everything begins here.<br>").arg(fn);
                } else {
                    result += QString("&bull; <code>%1()</code> — a helper function "
                                      "(called by other parts of the code).<br>").arg(fn);
                }
            }
            result += "<br>";
        }

        // Imports
        if (!a.imports.isEmpty()) {
            result += "<b>What it uses from outside:</b> ";
            QStringList tagged;
            for (const QString &imp : a.imports) tagged << "<code>" + imp + "</code>";
            result += tagged.join(", ") + ".<br><br>";
        }

        // Control flow
        if (loopCount > 0 || condCount > 0) {
            result += "<b>How it makes decisions:</b><br>";
            if (loopCount > 0)
                result += QString("&bull; It repeats steps %1 time%2 using loops.<br>")
                              .arg(loopCount).arg(loopCount > 1 ? "s" : "");
            if (condCount > 0)
                result += QString("&bull; It checks %1 condition%2 to decide what to do.<br>")
                              .arg(condCount).arg(condCount > 1 ? "s" : "");
            result += "<br>";
        }

        if (!a.entryPoint.isEmpty())
            result += QString("<b>Entry point:</b> the program starts at the <code>%1</code>.<br>")
                          .arg(a.entryPoint);
        result += "<br><i>Tip: click on any function name in the editor to see where it's used.</i>";
    }

    // ── Level 2 — Intermediate ────────────────────────────────────────────
    else if (level == 2) {
        result += QString("<b>Walkthrough — <code>%1</code>:</b><br><br>").arg(filename);

        if (!a.entryPoint.isEmpty())
            result += "<b>Entry point:</b> <code>" + a.entryPoint + "</code><br><br>";

        if (!a.functions.isEmpty()) {
            result += "<b>Functions:</b><br>";
            for (const QString &fn : a.functions) {
                result += QString("&bull; <code>%1()</code>").arg(fn);
                // Try to hint at role from name patterns
                if (fn.contains("init", Qt::CaseInsensitive) || fn.contains("setup", Qt::CaseInsensitive))
                    result += " — initialization / setup";
                else if (fn.contains("main", Qt::CaseInsensitive))
                    result += " — program entry point";
                else if (fn.contains("get", Qt::CaseInsensitive) || fn.contains("fetch", Qt::CaseInsensitive))
                    result += " — retrieves data";
                else if (fn.contains("set", Qt::CaseInsensitive) || fn.contains("update", Qt::CaseInsensitive))
                    result += " — modifies state";
                else if (fn.contains("calc", Qt::CaseInsensitive) || fn.contains("compute", Qt::CaseInsensitive)
                         || fn.contains("total", Qt::CaseInsensitive) || fn.contains("sum", Qt::CaseInsensitive))
                    result += " — computes a value";
                else if (fn.contains("print", Qt::CaseInsensitive) || fn.contains("display", Qt::CaseInsensitive)
                         || fn.contains("show", Qt::CaseInsensitive))
                    result += " — outputs to screen";
                else if (fn.contains("parse", Qt::CaseInsensitive) || fn.contains("read", Qt::CaseInsensitive))
                    result += " — reads / parses input";
                result += "<br>";
            }
            result += "<br>";
        }

        if (!a.classes.isEmpty()) {
            result += "<b>Types/Classes:</b> ";
            QStringList tagged;
            for (const QString &c : a.classes) tagged << "<code>" + c + "</code>";
            result += tagged.join(", ") + "<br><br>";
        }

        if (loopCount > 0) {
            result += QString("<b>Loops:</b> %1 loop%2 found — iterates over data or repeats until a condition.<br>")
                          .arg(loopCount).arg(loopCount > 1 ? "s" : "");
        }
        if (condCount > 0) {
            result += QString("<b>Conditionals:</b> %1 branch%2 — guards, early returns, or decision paths.<br>")
                          .arg(condCount).arg(condCount > 1 ? "es" : "");
        }
        if (!a.imports.isEmpty()) {
            result += "<br><b>Dependencies:</b> ";
            QStringList tagged;
            for (const QString &imp : a.imports) tagged << "<code>" + imp + "</code>";
            result += tagged.join(", ") + "<br>";
        }

        result += QString("<br><b>Size:</b> %1 lines, %2 variable assignment%3.")
                      .arg(a.lineCount).arg(a.varCount).arg(a.varCount != 1 ? "s" : "");
    }

    // ── Level 3 — Developer ───────────────────────────────────────────────
    else if (level == 3) {
        result += QString("<b><code>%1</code> — structural analysis:</b><br><br>").arg(filename);

        // Call graph
        result += "<b>Call graph:</b> " + callGraph(a.functions) + "<br>";

        if (!a.entryPoint.isEmpty())
            result += "<b>Entry:</b> <code>" + a.entryPoint + "</code><br>";

        result += QString("<b>Scope:</b> %1 line%2, %3 fn%4, %5 class%6, %7 import%8<br>")
                      .arg(a.lineCount).arg(a.lineCount != 1 ? "s" : "")
                      .arg(fnCount).arg(fnCount != 1 ? "s" : "")
                      .arg(classCount).arg(classCount != 1 ? "es" : "")
                      .arg(importCount).arg(importCount != 1 ? "s" : "");

        result += QString("<b>Control:</b> %1 loop%2, %3 branch%4, %5 return stmt%6<br>")
                      .arg(loopCount).arg(loopCount != 1 ? "s" : "")
                      .arg(condCount).arg(condCount != 1 ? "es" : "")
                      .arg(a.returns.size()).arg(a.returns.size() != 1 ? "s" : "");

        if (!a.imports.isEmpty()) {
            QStringList tagged;
            for (const QString &imp : a.imports) tagged << "<code>" + imp + "</code>";
            result += "<b>Deps:</b> " + tagged.join(", ") + "<br>";
        }

        // Complexity hint
        int complexity = loopCount + condCount + a.returns.size();
        QString complexLabel = complexity <= 3 ? "low" : (complexity <= 10 ? "moderate" : "high");
        result += "<b>Cyclomatic complexity (est.):</b> " + complexLabel;
        result += QString(" (%1 branch point%2)<br>").arg(complexity).arg(complexity != 1 ? "s" : "");

        // Language-specific note
        if (a.lang == "c") result += "<br><i>C: manual memory management — check malloc/free pairing.</i>";
        else if (a.lang == "cpp") result += "<br><i>C++: RAII applies — destructors run on scope exit.</i>";
        else if (a.lang == "python") result += "<br><i>Python: GC + GIL — threaded IO-bound is fine, CPU-bound is not.</i>";
        else if (a.lang == "rust") result += "<br><i>Rust: borrow checker enforces memory safety at compile time.</i>";
    }

    // ── Level 4 — No Assist ───────────────────────────────────────────────
    else {
        result += QString("<b><code>%1</code>:</b> ").arg(filename);
        QStringList taggedFns;
        for (const QString &fn : a.functions) taggedFns << "<code>" + fn + "()</code>";
        result += "Functions: " + (taggedFns.isEmpty() ? "none" : taggedFns.join(", ")) + ". ";

        if (!a.classes.isEmpty()) {
            QStringList tagged;
            for (const QString &c : a.classes) tagged << "<code>" + c + "</code>";
            result += "Types: " + tagged.join(", ") + ". ";
        }

        result += QString("%1L, %2B, %3L, %4I.")
                      .arg(a.lineCount)
                      .arg(condCount)
                      .arg(loopCount)
                      .arg(importCount);
        if (!a.entryPoint.isEmpty())
            result += " Entry: <code>" + a.entryPoint + "</code>.";
    }

    return result;
}
