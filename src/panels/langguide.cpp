#include "panels/langguide.h"
#include "core/jsonloader.h"
#include <QKeyEvent>
#include <QToolTip>
#include <QMap>
#include <QCursor>
#include <QRegularExpression>
#include <QJsonArray>
#include <QJsonObject>

// ============================================================================
// Theme colors
// ============================================================================
static const char* BG      = "#1e1e2e";
static const char* BG3     = "#333348";
static const char* BG4     = "#3c3c54";
static const char* FG      = "#cdd6f4";
static const char* FG2     = "#a6adc8";
static const char* FG3     = "#6c7086";
static const char* ACCENT  = "#89b4fa";
static const char* TEAL    = "#94e2d5";
static const char* GREEN   = "#a6e3a1";
static const char* RED     = "#f38ba8";
static const char* YELLOW  = "#f9e2af";
static const char* BORDER  = "#45475a";

// ============================================================================
// LangGuideOverlay
// ============================================================================

LangGuideOverlay::LangGuideOverlay(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(580);
    setMaximumHeight(600);
    setStyleSheet(QString(
        "LangGuideOverlay { background: %1; border: 1px solid %2; "
        "border-radius: 6px; }"
    ).arg(BG3, BORDER));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(4);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet(QString(
        "font-size: 15px; font-weight: 700; color: %1;"
    ).arg(ACCENT));
    mainLayout->addWidget(m_titleLabel);

    m_subtitleLabel = new QLabel(this);
    m_subtitleLabel->setStyleSheet(QString(
        "font-size: 11px; color: %1; margin-bottom: 16px;"
    ).arg(FG3));
    m_subtitleLabel->setWordWrap(true);
    mainLayout->addWidget(m_subtitleLabel);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 7px; background: transparent; }"
        "QScrollBar::handle:vertical { background: #3c3c54; border-radius: 4px; }"
    );

    m_bodyWidget = new QWidget;
    m_bodyLayout = new QVBoxLayout(m_bodyWidget);
    m_bodyLayout->setContentsMargins(0, 0, 0, 0);
    m_bodyLayout->setSpacing(0);
    m_scrollArea->setWidget(m_bodyWidget);
    mainLayout->addWidget(m_scrollArea, 1);

    m_closeButton = new QPushButton("X", this);
    m_closeButton->setFixedSize(24, 24);
    m_closeButton->setStyleSheet(QString(
        "QPushButton { background: none; color: %1; font-size: 16px; border: none; }"
        "QPushButton:hover { color: %2; }"
    ).arg(FG3, FG));
    m_closeButton->move(width() - 36, 12);
    connect(m_closeButton, &QPushButton::clicked, this, &LangGuideOverlay::closeRequested);

    setVisible(false);
}

void LangGuideOverlay::showForLanguage(const QString &lang)
{
    LangGuideData data = loadLanguageData(lang);
    if (data.title.isEmpty()) return;
    buildGuide(data);
    setVisible(true);
    raise();
}

// ============================================================================
// JSON loading
// ============================================================================

LangGuideData LangGuideOverlay::loadLanguageData(const QString &lang)
{
    QJsonArray languages = JsonLoader::loadArray("langguide.json", "languages");
    for (const QJsonValue &lv : languages) {
        QJsonObject lo = lv.toObject();
        if (lo["id"].toString() != lang) continue;

        LangGuideData data;
        data.title    = lo["title"].toString();
        data.subtitle = lo["subtitle"].toString();

        for (const QJsonValue &sv : lo["sections"].toArray()) {
            QJsonObject so = sv.toObject();
            LangGuideSection section;
            section.name = so["name"].toString();
            for (const QJsonValue &rv : so["rows"].toArray()) {
                QJsonObject ro = rv.toObject();
                LangGuideRow row;
                row.label    = ro["label"].toString();
                row.status   = ro["status"].toString();
                row.value    = ro["value"].toString();
                row.helpText = ro["helpText"].toString();
                section.rows.append(row);
            }
            data.sections.append(section);
        }
        return data;
    }
    return {};
}

void LangGuideOverlay::buildGuide(const LangGuideData &data)
{
    m_titleLabel->setText(data.title);
    m_subtitleLabel->setText(data.subtitle);

    // Clear old body
    QLayoutItem *child;
    while ((child = m_bodyLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    for (const auto &section : data.sections) {
        m_bodyLayout->addWidget(createSectionHeader(section.name));
        for (const auto &row : section.rows) {
            m_bodyLayout->addWidget(createRow(row));
        }
    }
    m_bodyLayout->addStretch();
}

QWidget* LangGuideOverlay::createSectionHeader(const QString &name)
{
    auto *label = new QLabel(name);
    label->setStyleSheet(QString(
        "font-size: 11px; font-weight: 700; color: %1; text-transform: uppercase; "
        "letter-spacing: 0.5px; margin: 14px 0 6px; padding-top: 10px; "
        "border-top: 1px solid %2;"
    ).arg(TEAL, BORDER));
    return label;
}

QWidget* LangGuideOverlay::createRow(const LangGuideRow &row)
{
    auto *widget = new QWidget;
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 5, 0, 5);
    layout->setSpacing(8);

    auto *label = new QLabel(row.label);
    label->setFixedWidth(160);
    label->setStyleSheet(QString("font-size: 12px; color: %1;").arg(FG2));
    layout->addWidget(label);

    auto *value = new QLabel(row.value);
    value->setStyleSheet(QString("font-size: 12px; font-weight: 600; color: %1;").arg(
        row.status == "yes"     ? GREEN :
        row.status == "no"      ? RED :
        row.status == "partial" ? YELLOW : FG
    ));
    layout->addWidget(value, 1);

    if (!row.helpText.isEmpty()) {
        auto *helpBtn = new QPushButton("?", widget);
        helpBtn->setFixedSize(18, 18);
        helpBtn->setCursor(Qt::PointingHandCursor);
        helpBtn->setStyleSheet(QString(
            "QPushButton { background: %1; color: %2; font-size: 9px; "
            "border-radius: 9px; border: none; }"
            "QPushButton:hover { color: %3; background: %4; }"
        ).arg(BG, FG3, ACCENT, BG4));

        QString helpTitle = row.label;
        QString helpBody  = row.helpText;
        connect(helpBtn, &QPushButton::clicked, this,
                [this, helpTitle, helpBody, helpBtn]() {
            showHelpTooltip(helpTitle, helpBody, helpBtn);
        });
        layout->addWidget(helpBtn);
    }

    return widget;
}

void LangGuideOverlay::showHelpTooltip(const QString &title, const QString &body,
                                        QWidget *anchor)
{
    QString tooltip = QString("<b style='color:%1'>%2</b><br>%3")
                          .arg(ACCENT, title, body);
    QToolTip::showText(anchor->mapToGlobal(QPoint(0, anchor->height() + 4)),
                       tooltip, anchor);
}

void LangGuideOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit closeRequested();
    }
    QWidget::keyPressEvent(event);
}

// ============================================================================
// Static factory methods — kept for API compatibility, load from JSON
// ============================================================================

LangGuideData LangGuideOverlay::pythonGuide() { return loadLanguageData("python"); }
LangGuideData LangGuideOverlay::cGuide()      { return loadLanguageData("c"); }
LangGuideData LangGuideOverlay::ulGuide()     { return loadLanguageData("ul"); }

// ============================================================================
// ExplainHandler
// ============================================================================

ExplainHandler::ExplainHandler(QObject *parent)
    : QObject(parent)
{
    initExplanations();
}

void ExplainHandler::initExplanations()
{
    QJsonObject examples = JsonLoader::loadObject("langguide.json");
    QJsonObject explainObj = examples["explain_examples"].toObject();
    for (const QString &key : explainObj.keys()) {
        m_explanations[key] = explainObj[key].toString();
    }
}

QString ExplainHandler::getExplainedCode(const QString &filename) const
{
    return m_explanations.value(filename);
}

bool ExplainHandler::hasExplanation(const QString &filename) const
{
    return m_explanations.contains(filename);
}

// ── Local line-by-line annotator ─────────────────────────────────────────────

static QString classifyLine(const QString &trimmed, const QString &lang, int level)
{
    if (trimmed.isEmpty()) return {};

    bool isPy = (lang == "python" || lang == "ul");
    bool isC  = (lang == "c" || lang == "cpp" || lang == "rust" || lang == "js");

    if (trimmed.startsWith("//") || trimmed.startsWith("/*") || trimmed.startsWith("*"))
        return {};
    if (isPy && trimmed.startsWith("#")) return {};
    if ((trimmed == "}" || trimmed == "{" || trimmed == "};") && level >= 3) return {};

    {
        QRegularExpression pyImport(R"(^(?:import|from)\s+(\S+))");
        auto m = pyImport.match(trimmed);
        if (isPy && m.hasMatch()) {
            QString mod = m.captured(1);
            if (level == 1) return QString("imports the \"%1\" module so we can use its functions").arg(mod);
            if (level == 2) return QString("import: %1").arg(mod);
            return {};
        }
    }
    {
        QRegularExpression inc(R"(^#include\s*[<"]([\w./]+)[>"])");
        auto m = inc.match(trimmed);
        if (isC && m.hasMatch()) {
            QString hdr = m.captured(1);
            if (level == 1) return QString("includes the \"%1\" header to access its functions").arg(hdr);
            if (level == 2) return QString("include: %1").arg(hdr);
            return {};
        }
    }

    {
        QRegularExpression pyDef(R"(^(?:def|fn|func)\s+(\w+)\s*\(([^)]*)\))");
        auto m = pyDef.match(trimmed);
        if (isPy && m.hasMatch()) {
            QString name = m.captured(1);
            QString params = m.captured(2).trimmed();
            if (level == 1) {
                if (params.isEmpty()) return QString("defines a function called \"%1\" that takes no inputs").arg(name);
                return QString("defines a function called \"%1\" — inputs: %2").arg(name, params);
            }
            if (level == 2) return QString("function %1(%2)").arg(name, params);
            if (level == 3) return QString("fn %1 | params: %2").arg(name, params.isEmpty() ? "none" : params);
            return {};
        }
    }
    {
        QRegularExpression cDef(R"(^\s*(?:static\s+|inline\s+|extern\s+)?(?:[\w:*&<>\[\]]+\s+)+(\w+)\s*\([^;]*$)");
        auto m = cDef.match(trimmed);
        if (isC && m.hasMatch() && !trimmed.endsWith(';')) {
            QString name = m.captured(1);
            static QStringList kw = {"if","for","while","switch","return","else","do","case"};
            if (!kw.contains(name)) {
                if (level == 1) return QString("defines the \"%1\" function").arg(name);
                if (level == 2) return QString("function: %1").arg(name);
                if (level == 3) return QString("fn %1").arg(name);
                return {};
            }
        }
    }

    {
        QRegularExpression classDef(R"(^(?:class|struct|enum)\s+(\w+))");
        auto m = classDef.match(trimmed);
        if (m.hasMatch()) {
            QString name = m.captured(1);
            QString kind = trimmed.startsWith("class") ? "class" :
                           trimmed.startsWith("struct") ? "struct" : "enum";
            if (level == 1) return QString("defines a %1 called \"%2\" — a blueprint for creating objects").arg(kind, name);
            if (level == 2) return QString("%1 definition: %2").arg(kind, name);
            return {};
        }
    }

    {
        QRegularExpression forLoop(R"(^for\s+(\w+)\s+in\s+(.+):?\s*$)");
        auto m = forLoop.match(trimmed);
        if (isPy && m.hasMatch()) {
            QString var = m.captured(1);
            QString iterable = m.captured(2).trimmed().remove(':');
            if (level == 1) return QString("loop: for each \"%1\" in \"%2\", run the indented block below").arg(var, iterable);
            if (level == 2) return QString("iterate %1 over %2").arg(var, iterable);
            return {};
        }
    }
    {
        QRegularExpression forC(R"(^for\s*\()");
        if (isC && forC.match(trimmed).hasMatch()) {
            if (level == 1) return "loop: repeats the block below, counting with the loop variable";
            if (level == 2) return "for loop";
            return {};
        }
    }
    {
        QRegularExpression whileRx(R"(^while\s*[\(])");
        if (isC && whileRx.match(trimmed).hasMatch()) {
            if (level == 1) return "loop: keeps repeating while the condition inside ( ) is true";
            if (level == 2) return "while loop";
            return {};
        }
        QRegularExpression whilePy(R"(^while\s+.+:)");
        if (isPy && whilePy.match(trimmed).hasMatch()) {
            if (level == 1) return "loop: repeats while the condition is true";
            if (level == 2) return "while loop";
            return {};
        }
    }

    {
        QRegularExpression ifRx(R"(^if\s+(.+?)(?:\s*:|\s*\{)?\s*$)");
        auto m = ifRx.match(trimmed);
        if (m.hasMatch()) {
            QString cond = m.captured(1).trimmed().remove(':').remove('{');
            if (level == 1) return QString("checks: if \"%1\" is true, run the block below").arg(cond);
            if (level == 2) return QString("condition: %1").arg(cond);
            return {};
        }
    }
    if (trimmed.startsWith("elif ") || trimmed.startsWith("else if")) {
        if (level == 1) return "otherwise, checks a different condition";
        if (level == 2) return "else if branch";
        return {};
    }
    if (trimmed == "else:" || trimmed == "else {" || trimmed == "else") {
        if (level == 1) return "if none of the above conditions matched, run this block";
        if (level == 2) return "fallback else branch";
        return {};
    }

    {
        QRegularExpression retRx(R"(^return\s*(.*))");
        auto m = retRx.match(trimmed);
        if (m.hasMatch()) {
            QString val = m.captured(1).trimmed().remove(';');
            if (level == 1) {
                if (val.isEmpty()) return "exits the function, returning nothing";
                return QString("exits the function and sends back \"%1\"").arg(val);
            }
            if (level == 2) return QString("return %1").arg(val.isEmpty() ? "void" : val);
            return {};
        }
    }

    {
        QRegularExpression printRx(R"(^(?:print|printf|println|console\.log|puts|cout)\b)");
        if (printRx.match(trimmed).hasMatch()) {
            if (level == 1) return "prints output to the screen for the user to see";
            if (level == 2) return "output to console";
            return {};
        }
    }

    {
        QRegularExpression pyAssign(R"(^(\w+)(?:\s*:\s*\w+)?\s*=\s*(.+))");
        auto m = pyAssign.match(trimmed);
        if (isPy && m.hasMatch() && !trimmed.contains("==") && !trimmed.startsWith("if")) {
            QString name = m.captured(1);
            QString val  = m.captured(2).trimmed();
            if (level == 1) return QString("creates a variable called \"%1\" and sets it to %2").arg(name, val);
            if (level == 2) return QString("%1 = %2").arg(name, val);
            return {};
        }
    }
    {
        QRegularExpression cDecl(R"(^(?:int|float|double|char|bool|auto|long|short|unsigned|const|string|QString|var|let|const)\s+(\w+)\s*(?:=\s*(.+?))?;)");
        auto m = cDecl.match(trimmed);
        if (isC && m.hasMatch()) {
            QString name = m.captured(1);
            QString val  = m.captured(2).trimmed();
            if (level == 1) {
                if (val.isEmpty()) return QString("declares a variable called \"%1\"").arg(name);
                return QString("declares \"%1\" and sets it to %2").arg(name, val);
            }
            if (level == 2) return QString("var %1%2").arg(name, val.isEmpty() ? "" : " = " + val);
            return {};
        }
    }

    {
        QRegularExpression callRx(R"(^(\w+)\s*\()");
        auto m = callRx.match(trimmed);
        if (m.hasMatch()) {
            QString fn = m.captured(1);
            static QStringList skip = {"if","for","while","switch","return","def","fn","func","class","struct"};
            if (!skip.contains(fn)) {
                if (level == 1) return QString("calls the \"%1\" function").arg(fn);
                if (level == 2) return QString("call: %1()").arg(fn);
                return {};
            }
        }
    }

    return {};
}

QString ExplainHandler::generateExplainedCode(const QString &code,
                                               const QString &lang,
                                               int level)
{
    if (code.trimmed().isEmpty()) return code;

    bool isPy = (lang == "python" || lang == "ul");
    QString commentPrefix = isPy ? "# " : "// ";

    QStringList lines = code.split('\n');
    QStringList result;

    QString levelName;
    if (level == 1) levelName = "Beginner";
    else if (level == 2) levelName = "Intermediate";
    else if (level == 3) levelName = "Developer";
    else levelName = "Signature-only";

    result << commentPrefix + QString("── Explained by Code Clarity (Level %1: %2) ──")
                                  .arg(level).arg(levelName);
    result << "";

    for (const QString &rawLine : lines) {
        QString trimmed = rawLine.trimmed();
        QString explanation = classifyLine(trimmed, lang, level);

        if (!explanation.isEmpty()) {
            QString indent;
            for (const QChar &ch : rawLine) {
                if (ch == ' ' || ch == '\t') indent += ch;
                else break;
            }
            result << indent + commentPrefix + explanation;
        }
        result << rawLine;
    }

    return result.join('\n');
}
