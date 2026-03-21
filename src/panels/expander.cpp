#include "panels/expander.h"
#include "core/jsonloader.h"
#include "core/theme.h"
#include <QKeyEvent>
#include <QRegularExpression>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>

using namespace Theme::Css;

// ── ExpanderOverlay ───────────────────────────────────────────────────────

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
    m_titleLabel->setStyleSheet(QString("font-size: 13px; font-weight: 700; color: %1;").arg(ACCENT));
    mainLayout->addWidget(m_titleLabel);

    m_descLabel = new QLabel(
        "Breaks compressed/nested statements into one operation per line.", this);
    m_descLabel->setStyleSheet(QString("font-size: 11px; color: %1;").arg(FG2));
    m_descLabel->setWordWrap(true);
    mainLayout->addWidget(m_descLabel);

    m_demoFrame = new QFrame(this);
    m_demoFrame->setStyleSheet(QString(
        "QFrame { background: %1; border-radius: 6px; padding: 12px; }").arg(BG));

    auto *demoLayout = new QVBoxLayout(m_demoFrame);
    demoLayout->setContentsMargins(12, 12, 12, 12);
    demoLayout->setSpacing(8);

    m_originalLabel = new QLabel(m_demoFrame);
    m_originalLabel->setStyleSheet(QString(
        "font-family: 'Cascadia Code', 'Fira Code', 'Consolas', monospace; "
        "font-size: 11px; color: %1;").arg(FG3));
    m_originalLabel->setWordWrap(true);
    demoLayout->addWidget(m_originalLabel);

    m_arrowLabel = new QLabel(m_demoFrame);
    m_arrowLabel->setText(QString::fromUtf8("\xe2\x86\x93"));
    m_arrowLabel->setAlignment(Qt::AlignCenter);
    m_arrowLabel->setStyleSheet(QString("font-size: 18px; color: %1;").arg(FG3));
    demoLayout->addWidget(m_arrowLabel);

    m_expandedLabel = new QLabel(m_demoFrame);
    m_expandedLabel->setStyleSheet(QString(
        "font-family: 'Cascadia Code', 'Fira Code', 'Consolas', monospace; "
        "font-size: 11px; color: %1;").arg(GREEN));
    m_expandedLabel->setWordWrap(true);
    demoLayout->addWidget(m_expandedLabel);

    mainLayout->addWidget(m_demoFrame);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();

    m_expandButton = new QPushButton("Expand Current File", this);
    m_expandButton->setStyleSheet(QString(
        "QPushButton { background: %1; color: #1e1e2e; font-weight: 600; "
        "padding: 5px 14px; border-radius: 6px; font-size: 11px; }").arg(PEACH));
    connect(m_expandButton, &QPushButton::clicked, this, &ExpanderOverlay::expandFileRequested);
    buttonRow->addWidget(m_expandButton);

    m_closeButton = new QPushButton("Close", this);
    m_closeButton->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; padding: 5px 14px; "
        "border-radius: 6px; font-size: 11px; }").arg(BG4, FG2));
    connect(m_closeButton, &QPushButton::clicked, this, &ExpanderOverlay::closeRequested);
    buttonRow->addWidget(m_closeButton);

    mainLayout->addLayout(buttonRow);
    setVisible(false);
}

void ExpanderOverlay::showWithDemo(const QString &original, const QString &expanded)
{
    m_originalLabel->setText(original);
    m_expandedLabel->setTextFormat(Qt::RichText);
    m_expandedLabel->setText(QString(
        "<span style='color:%1; font-style:italic'># -- Expanded by Clarity --</span><br>"
        "%2<br>"
        "<span style='color:%1; font-style:italic'># -- End expansion --</span>"
    ).arg(FG3, expanded));
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
    if (event->key() == Qt::Key_Escape) emit closeRequested();
    QWidget::keyPressEvent(event);
}

// ── WalkThroughHandler ────────────────────────────────────────────────────

WalkThroughHandler::WalkThroughHandler(QObject *parent)
    : QObject(parent)
{
}

struct CodeAnalysis {
    QStringList functions, classes, imports, loops, conditionals, returns;
    QString entryPoint, lang;
    int lineCount = 0, varCount = 0;
};

static CodeAnalysis analyzeCode(const QString &code, const QString &filename)
{
    CodeAnalysis a;
    QStringList lines = code.split('\n');
    a.lineCount = lines.size();

    QString ext;
    int dot = filename.lastIndexOf('.');
    if (dot >= 0) ext = filename.mid(dot + 1).toLower();
    if      (ext == "py")                                      a.lang = "python";
    else if (ext == "c" || ext == "h")                         a.lang = "c";
    else if (ext == "cpp" || ext == "cxx" || ext == "cc" || ext == "hpp") a.lang = "cpp";
    else if (ext == "rs")                                      a.lang = "rust";
    else if (ext == "ul")                                      a.lang = "ul";
    else if (ext == "js" || ext == "ts")                       a.lang = "js";
    else                                                       a.lang = "text";

    QRegularExpression pyFunc(R"(^\s*def\s+(\w+)\s*\()");
    QRegularExpression pyClass(R"(^\s*class\s+(\w+))");
    QRegularExpression pyImport(R"(^\s*(?:import|from)\s+([\w\.]+))");
    QRegularExpression cInclude(R"(^\s*#include\s*[<"]([\w./]+)[>"])");
    QRegularExpression cClass(R"(^\s*(?:struct|class|enum)\s+(\w+))");
    QRegularExpression loopRx(R"(^\s*(for|while|foreach)\b)");
    QRegularExpression condRx(R"(^\s*(if|elif|else|switch|case)\b)");
    QRegularExpression retRx(R"(^\s*return\b)");
    QRegularExpression varRx(R"(^\s*(?:let|var|const|auto)?\s*\w+\s*=\s*\S)");
    QRegularExpression pyMain(R"(if\s+__name__\s*==\s*['"']__main__['"'])");
    QRegularExpression mainFn(R"(\bmain\s*\()");
    QRegularExpression simpleFn(R"(^\s*(?:(?:static|inline|extern|pub|fn|func|async)\s+)*(?:[\w:*&<>\[\]]+\s+)+(\w+)\s*\()");
    static QStringList kw = {"if","for","while","switch","return","else","do","case","sizeof","alignof","typeof"};

    for (const QString &rawLine : lines) {
        QString trimmed = rawLine.trimmed();
        if (trimmed.isEmpty()) continue;
        if (trimmed.startsWith("//") || trimmed.startsWith("#!")) continue;
        if ((a.lang == "python" || a.lang == "ul") && trimmed.startsWith("#")) continue;

        if (pyMain.match(rawLine).hasMatch()) a.entryPoint = "__main__ guard";
        if (mainFn.match(rawLine).hasMatch() && a.entryPoint.isEmpty()) a.entryPoint = "main()";

        if (a.lang == "python" || a.lang == "ul") {
            auto m = pyFunc.match(rawLine);
            if (m.hasMatch() && !a.functions.contains(m.captured(1))) a.functions << m.captured(1);
            auto mc = pyClass.match(rawLine);
            if (mc.hasMatch() && !a.classes.contains(mc.captured(1))) a.classes << mc.captured(1);
            auto mi = pyImport.match(rawLine);
            if (mi.hasMatch() && !a.imports.contains(mi.captured(1))) a.imports << mi.captured(1);
        } else {
            auto m = simpleFn.match(rawLine);
            if (m.hasMatch()) {
                QString name = m.captured(1);
                if (!kw.contains(name) && !a.functions.contains(name)) a.functions << name;
            }
            auto mc = cClass.match(rawLine);
            if (mc.hasMatch() && !a.classes.contains(mc.captured(1))) a.classes << mc.captured(1);
            auto mi = cInclude.match(rawLine);
            if (mi.hasMatch() && !a.imports.contains(mi.captured(1))) a.imports << mi.captured(1);
        }

        if (loopRx.match(rawLine).hasMatch() && a.loops.size() < 6)       a.loops << trimmed.left(50);
        if (condRx.match(rawLine).hasMatch() && a.conditionals.size() < 6) a.conditionals << trimmed.left(50);
        if (retRx.match(rawLine).hasMatch() && a.returns.size() < 4)       a.returns << trimmed.left(60);
        if (varRx.match(rawLine).hasMatch()) a.varCount++;
    }
    return a;
}

static QString callGraph(const QStringList &fns)
{
    if (fns.isEmpty()) return "(no functions detected)";
    QStringList wrapped;
    for (const QString &f : fns) wrapped << "<code>" + f + "()</code>";
    if (wrapped.size() == 1) return wrapped.first();
    return wrapped.first() + " &rarr; " + wrapped.mid(1).join(", ");
}

static QString hintForFunction(const QString &name, const QJsonArray &hints)
{
    QString lower = name.toLower();
    for (const QJsonValue &v : hints) {
        QJsonObject h = v.toObject();
        if (lower.contains(h["contains"].toString()))
            return h["hint"].toString();
    }
    return {};
}

QString WalkThroughHandler::generateWalkthrough(const QString &filename,
                                                 const QString &code,
                                                 int level) const
{
    if (code.trimmed().isEmpty())
        return "<b>Walk Me Through:</b> The editor is empty. Open a file first.";

    CodeAnalysis a = analyzeCode(code, filename);
    QJsonObject cfg = JsonLoader::loadObject("patterns_expander.json");
    QJsonArray hints = cfg["function_hints"].toArray();
    QJsonObject langNotes = cfg["language_notes"].toObject();

    int fnCount = a.functions.size(), classCount = a.classes.size();
    int loopCount = a.loops.size(), condCount = a.conditionals.size();
    int importCount = a.imports.size();
    QString result;

    if (level == 1) {
        result += QString("<b>Walking you through <code>%1</code>:</b><br><br>").arg(filename);
        QStringList parts;
        if (fnCount > 0)     parts << QString("%1 function%2").arg(fnCount).arg(fnCount > 1 ? "s" : "");
        if (classCount > 0)  parts << QString("%1 class%2").arg(classCount).arg(classCount > 1 ? "es" : "");
        if (importCount > 0) parts << QString("%1 import%2").arg(importCount).arg(importCount > 1 ? "s" : "");
        result += "<b>Overview:</b> This program has " + (parts.isEmpty() ? "no named sections" : parts.join(", ")) + ".<br><br>";

        if (!a.functions.isEmpty()) {
            result += "<b>What each function does:</b><br>";
            for (const QString &fn : a.functions) {
                if (fn == "main")
                    result += QString("&bull; <code>%1()</code> — this is where the program <b>starts</b>.<br>").arg(fn);
                else
                    result += QString("&bull; <code>%1()</code> — a helper function.<br>").arg(fn);
            }
            result += "<br>";
        }
        if (!a.imports.isEmpty()) {
            QStringList tagged;
            for (const QString &imp : a.imports) tagged << "<code>" + imp + "</code>";
            result += "<b>What it uses from outside:</b> " + tagged.join(", ") + ".<br><br>";
        }
        if (loopCount > 0 || condCount > 0) {
            result += "<b>How it makes decisions:</b><br>";
            if (loopCount > 0) result += QString("&bull; Repeats steps %1 time%2 using loops.<br>").arg(loopCount).arg(loopCount > 1 ? "s" : "");
            if (condCount > 0) result += QString("&bull; Checks %1 condition%2.<br>").arg(condCount).arg(condCount > 1 ? "s" : "");
            result += "<br>";
        }
        if (!a.entryPoint.isEmpty())
            result += QString("<b>Entry point:</b> <code>%1</code>.<br>").arg(a.entryPoint);
        result += "<br><i>Tip: click any function name in the editor to see where it's used.</i>";

    } else if (level == 2) {
        result += QString("<b>Walkthrough — <code>%1</code>:</b><br><br>").arg(filename);
        if (!a.entryPoint.isEmpty()) result += "<b>Entry point:</b> <code>" + a.entryPoint + "</code><br><br>";
        if (!a.functions.isEmpty()) {
            result += "<b>Functions:</b><br>";
            for (const QString &fn : a.functions) {
                result += QString("&bull; <code>%1()</code>").arg(fn);
                QString hint = hintForFunction(fn, hints);
                if (!hint.isEmpty()) result += " — " + hint;
                result += "<br>";
            }
            result += "<br>";
        }
        if (!a.classes.isEmpty()) {
            QStringList tagged;
            for (const QString &c : a.classes) tagged << "<code>" + c + "</code>";
            result += "<b>Types/Classes:</b> " + tagged.join(", ") + "<br><br>";
        }
        if (loopCount > 0) result += QString("<b>Loops:</b> %1 loop%2 found.<br>").arg(loopCount).arg(loopCount > 1 ? "s" : "");
        if (condCount > 0) result += QString("<b>Conditionals:</b> %1 branch%2.<br>").arg(condCount).arg(condCount > 1 ? "es" : "");
        if (!a.imports.isEmpty()) {
            QStringList tagged;
            for (const QString &imp : a.imports) tagged << "<code>" + imp + "</code>";
            result += "<br><b>Dependencies:</b> " + tagged.join(", ") + "<br>";
        }
        result += QString("<br><b>Size:</b> %1 lines, %2 variable assignment%3.")
                      .arg(a.lineCount).arg(a.varCount).arg(a.varCount != 1 ? "s" : "");

    } else if (level == 3) {
        result += QString("<b><code>%1</code> — structural analysis:</b><br><br>").arg(filename);
        result += "<b>Call graph:</b> " + callGraph(a.functions) + "<br>";
        if (!a.entryPoint.isEmpty()) result += "<b>Entry:</b> <code>" + a.entryPoint + "</code><br>";
        result += QString("<b>Scope:</b> %1L, %2 fn, %3 class, %4 import<br>")
                      .arg(a.lineCount).arg(fnCount).arg(classCount).arg(importCount);
        int complexity = loopCount + condCount + a.returns.size();
        result += QString("<b>Control:</b> %1 loop, %2 branch, %3 return<br>")
                      .arg(loopCount).arg(condCount).arg(a.returns.size());
        if (!a.imports.isEmpty()) {
            QStringList tagged;
            for (const QString &imp : a.imports) tagged << "<code>" + imp + "</code>";
            result += "<b>Deps:</b> " + tagged.join(", ") + "<br>";
        }
        QString complexLabel = complexity <= 3 ? "low" : (complexity <= 10 ? "moderate" : "high");
        result += "<b>Cyclomatic complexity (est.):</b> " + complexLabel +
                  QString(" (%1 branch point%2)<br>").arg(complexity).arg(complexity != 1 ? "s" : "");
        QString note = langNotes[a.lang].toString();
        if (!note.isEmpty()) result += "<br><i>" + note + "</i>";

    } else {
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
                      .arg(a.lineCount).arg(condCount).arg(loopCount).arg(importCount);
        if (!a.entryPoint.isEmpty()) result += " Entry: <code>" + a.entryPoint + "</code>.";
    }

    return result;
}
