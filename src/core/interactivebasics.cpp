#include "core/interactivebasics.h"
#include "editor/highlighter.h"

#include <QScrollArea>
#include <QScrollBar>
#include <QTextDocument>
#include <QMap>
#include <QSizePolicy>
#include <QApplication>
#include <QGroupBox>
#include <QSpacerItem>

// ============================================================================
// Theme constants
// ============================================================================
static const char* IB_BG      = "#1e1e2e";
static const char* IB_BG2     = "#2a2a3c";
static const char* IB_BG3     = "#333348";
static const char* IB_FG      = "#cdd6f4";
static const char* IB_FG2     = "#a6adc8";
static const char* IB_FG3     = "#6c7086";
static const char* IB_ACCENT  = "#89b4fa";
static const char* IB_GREEN   = "#a6e3a1";
static const char* IB_RED     = "#f38ba8";
static const char* IB_YELLOW  = "#f9e2af";
static const char* IB_BORDER  = "#45475a";

// ============================================================================
// Constructor
// ============================================================================

InteractiveBasicsDialog::InteractiveBasicsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Interactive Basics Tutorial");
    setMinimumSize(720, 560);
    resize(820, 640);
    setModal(true);

    setStyleSheet(QString(
        "QDialog { background: %1; }"
        "QLabel  { color: %2; }"
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 7px; background: transparent; }"
        "QScrollBar::handle:vertical { background: #3c3c54; border-radius: 4px; min-height: 20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
    ).arg(IB_BG, IB_FG));

    // Initialise exercise slot vector (9 entries, index 0 unused, 8 = done)
    m_topicExercises.resize(TOTAL_STEPS);

    buildUI();
    updateProgress();
    updateNavButtons();
}

// ============================================================================
// UI Construction
// ============================================================================

void InteractiveBasicsDialog::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Progress bar ──────────────────────────────────────────────────────
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, TOTAL_STEPS - 1);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(5);
    m_progressBar->setStyleSheet(QString(
        "QProgressBar { background: %1; border: none; border-radius: 0px; }"
        "QProgressBar::chunk { background: %2; border-radius: 0px; }"
    ).arg(IB_BG3, IB_ACCENT));
    root->addWidget(m_progressBar);

    // ── Stacked widget ────────────────────────────────────────────────────
    m_stack = new QStackedWidget(this);
    m_stack->setStyleSheet("background: transparent;");

    // Build all pages and add them
    m_stack->addWidget(buildLangPage());    // page 0 — language selection
    m_stack->addWidget(buildTopic1Page()); // page 1 — Types & Variables
    m_stack->addWidget(buildTopic2Page()); // page 2 — Functions
    m_stack->addWidget(buildTopic3Page()); // page 3 — If/Else
    m_stack->addWidget(buildTopic4Page()); // page 4 — Loops
    m_stack->addWidget(buildTopic5Page()); // page 5 — Collections
    m_stack->addWidget(buildTopic6Page()); // page 6 — Objects
    m_stack->addWidget(buildTopic7Page()); // page 7 — Deep Dive
    m_stack->addWidget(buildDonePage());   // page 8 — Done

    root->addWidget(m_stack, 1);

    // ── Nav bar ───────────────────────────────────────────────────────────
    buildNavBar(root);
}

void InteractiveBasicsDialog::buildNavBar(QVBoxLayout *mainLayout)
{
    auto *bar = new QWidget(this);
    bar->setFixedHeight(52);
    bar->setStyleSheet(QString(
        "QWidget { background: %1; border-top: 1px solid %2; }"
    ).arg(IB_BG2, IB_BORDER));

    auto *lay = new QHBoxLayout(bar);
    lay->setContentsMargins(20, 0, 20, 0);

    m_backBtn = new QPushButton("<- Back", bar);
    m_backBtn->setFixedHeight(32);
    m_backBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; padding: 6px 20px; "
        "font-size: 12px; border-radius: 6px; border: none; }"
        "QPushButton:hover { background: %3; }"
    ).arg(IB_BG3, IB_FG, IB_BORDER));
    connect(m_backBtn, &QPushButton::clicked, this, &InteractiveBasicsDialog::onBackClicked);

    m_nextBtn = new QPushButton("Next ->", bar);
    m_nextBtn->setFixedHeight(32);
    m_nextBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: #1e1e2e; padding: 6px 20px; "
        "font-size: 12px; font-weight: 700; border-radius: 6px; border: none; }"
        "QPushButton:disabled { background: %2; color: %3; }"
        "QPushButton:hover:!disabled { background: #a6c8ff; }"
    ).arg(IB_ACCENT, IB_BG3, IB_FG3));
    connect(m_nextBtn, &QPushButton::clicked, this, &InteractiveBasicsDialog::onNextClicked);

    lay->addWidget(m_backBtn);
    lay->addStretch();
    lay->addWidget(m_nextBtn);

    mainLayout->addWidget(bar);
}

// ============================================================================
// Utility builders
// ============================================================================

QLabel* InteractiveBasicsDialog::makeTeachingLabel(const QString &html, QWidget *parent)
{
    auto *lbl = new QLabel(html, parent);
    lbl->setWordWrap(true);
    lbl->setTextFormat(Qt::RichText);
    lbl->setStyleSheet(QString(
        "font-size: 13px; color: %1; line-height: 1.7; background: transparent;"
    ).arg(IB_FG2));
    lbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    return lbl;
}

QWidget* InteractiveBasicsDialog::makeCodeBlock(const QString &code, const QString &lang, QWidget *parent)
{
    auto *frame = new QFrame(parent);
    frame->setStyleSheet(QString(
        "QFrame { background: #12121c; border-radius: 6px; border-left: 3px solid %1; }"
    ).arg(IB_ACCENT));

    auto *lay = new QVBoxLayout(frame);
    lay->setContentsMargins(14, 10, 14, 10);
    lay->setSpacing(4);

    // Language label
    auto *langLbl = new QLabel(lang, frame);
    langLbl->setStyleSheet(QString("font-size: 10px; color: %1; font-weight: 700;").arg(IB_FG3));
    lay->addWidget(langLbl);

    auto *editor = new QPlainTextEdit(frame);
    editor->setReadOnly(true);
    editor->setPlainText(code);
    editor->setStyleSheet(
        "QPlainTextEdit { background: transparent; color: #cdd6f4; "
        "font-family: 'Consolas','Courier New',monospace; font-size: 12px; "
        "border: none; selection-background-color: #45475a; }"
    );
    editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Apply syntax highlighting
    auto *hl = new SyntaxHighlighter(editor->document());
    hl->setLanguage(lang);
    Q_UNUSED(hl);

    // Shrink to content
    int lineCount = code.count('\n') + 1;
    int lineHeight = editor->fontMetrics().lineSpacing();
    editor->setFixedHeight(qMin(lineCount * lineHeight + 14, 260));

    lay->addWidget(editor);
    return frame;
}

QPushButton* InteractiveBasicsDialog::makeCheckButton(QWidget *parent)
{
    auto *btn = new QPushButton("Check Answer", parent);
    btn->setFixedHeight(30);
    btn->setStyleSheet(QString(
        "QPushButton { background: %1; color: #1e1e2e; padding: 4px 18px; "
        "font-size: 12px; font-weight: 700; border-radius: 5px; border: none; }"
        "QPushButton:hover { background: #a6c8ff; }"
    ).arg(IB_ACCENT));
    return btn;
}

QLabel* InteractiveBasicsDialog::makeFeedbackLabel(QWidget *parent)
{
    auto *lbl = new QLabel("", parent);
    lbl->setWordWrap(true);
    lbl->setTextFormat(Qt::RichText);
    lbl->setStyleSheet("font-size: 12px; background: transparent; padding: 4px 0px;");
    lbl->setVisible(false);
    return lbl;
}

// ============================================================================
// Exercise factories
// ============================================================================

InteractiveBasicsDialog::ExerciseWidgets InteractiveBasicsDialog::makeRadioExercise(
    QWidget *parent, QVBoxLayout *layout,
    const QString &question,
    const QStringList &options,
    int correctIndex,
    const QString &explanation)
{
    ExerciseWidgets ex;
    ex.type = "radio";
    ex.correctRadio = correctIndex;

    // Separator line
    auto *sep = new QFrame(parent);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("border: 1px solid %1;").arg(IB_BORDER));
    layout->addWidget(sep);

    auto *qLbl = new QLabel("<b>Exercise:</b> " + question, parent);
    qLbl->setWordWrap(true);
    qLbl->setTextFormat(Qt::RichText);
    qLbl->setStyleSheet(QString("font-size: 13px; color: %1; background: transparent;").arg(IB_FG));
    layout->addWidget(qLbl);

    ex.radioGroup = new QButtonGroup(parent);

    for (int i = 0; i < options.size(); ++i) {
        auto *rb = new QRadioButton(options[i], parent);
        rb->setStyleSheet(QString(
            "QRadioButton { color: %1; font-size: 12px; background: transparent; spacing: 8px; }"
            "QRadioButton::indicator { width: 14px; height: 14px; }"
            "QRadioButton::indicator:unchecked { border: 2px solid %2; border-radius: 7px; background: transparent; }"
            "QRadioButton::indicator:checked { border: 2px solid %3; border-radius: 7px; background: %3; }"
        ).arg(IB_FG2, IB_BORDER, IB_ACCENT));
        ex.radioGroup->addButton(rb, i);
        ex.radios.append(rb);
        layout->addWidget(rb);
    }

    ex.feedbackLabel = makeFeedbackLabel(parent);
    layout->addWidget(ex.feedbackLabel);
    // Store explanation in the label's object name so checkCurrentExercise can retrieve it
    ex.feedbackLabel->setObjectName(explanation);

    ex.checkBtn = makeCheckButton(parent);
    layout->addWidget(ex.checkBtn);

    return ex;
}

InteractiveBasicsDialog::ExerciseWidgets InteractiveBasicsDialog::makeTextExercise(
    QWidget *parent, QVBoxLayout *layout,
    const QString &question,
    const QString &correct,
    const QString &explanation)
{
    ExerciseWidgets ex;
    ex.type = "text";
    ex.correctText = correct.trimmed().toLower();

    auto *sep = new QFrame(parent);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("border: 1px solid %1;").arg(IB_BORDER));
    layout->addWidget(sep);

    auto *qLbl = new QLabel("<b>Exercise:</b> " + question, parent);
    qLbl->setWordWrap(true);
    qLbl->setTextFormat(Qt::RichText);
    qLbl->setStyleSheet(QString("font-size: 13px; color: %1; background: transparent;").arg(IB_FG));
    layout->addWidget(qLbl);

    ex.lineEdit = new QLineEdit(parent);
    ex.lineEdit->setPlaceholderText("Type your answer here...");
    ex.lineEdit->setStyleSheet(QString(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3; "
        "border-radius: 5px; padding: 5px 10px; font-size: 12px; }"
        "QLineEdit:focus { border-color: %4; }"
    ).arg(IB_BG3, IB_FG, IB_BORDER, IB_ACCENT));
    layout->addWidget(ex.lineEdit);

    ex.feedbackLabel = makeFeedbackLabel(parent);
    layout->addWidget(ex.feedbackLabel);
    ex.feedbackLabel->setObjectName(explanation);

    ex.checkBtn = makeCheckButton(parent);
    layout->addWidget(ex.checkBtn);

    return ex;
}

// ============================================================================
// Page builders — helpers to wrap content in a QScrollArea
// ============================================================================

static QWidget* wrapInScroll(QWidget *content)
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 7px; background: transparent; }"
        "QScrollBar::handle:vertical { background: #3c3c54; border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
    );
    scroll->setWidget(content);
    return scroll;
}

static QWidget* makeSectionTitle(const QString &step, const QString &title, const QString &subtitle)
{
    auto *w = new QWidget;
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 8);
    lay->setSpacing(2);

    auto *stepLbl = new QLabel(step);
    stepLbl->setStyleSheet(QString(
        "font-size: 10px; font-weight: 700; color: %1; letter-spacing: 1px;"
    ).arg(IB_FG3));
    lay->addWidget(stepLbl);

    auto *titleLbl = new QLabel(title);
    titleLbl->setStyleSheet(QString(
        "font-size: 20px; font-weight: 700; color: %1;"
    ).arg(IB_ACCENT));
    lay->addWidget(titleLbl);

    auto *subLbl = new QLabel(subtitle);
    subLbl->setWordWrap(true);
    subLbl->setStyleSheet(QString("font-size: 12px; color: %1;").arg(IB_FG3));
    lay->addWidget(subLbl);

    return w;
}

// ============================================================================
// Page 0 — Language Selection
// ============================================================================

QWidget* InteractiveBasicsDialog::buildLangPage()
{
    auto *outer = new QWidget;
    auto *outerLay = new QVBoxLayout(outer);
    outerLay->setContentsMargins(0, 0, 0, 0);

    auto *inner = new QWidget;
    inner->setMaximumWidth(600);
    auto *lay = new QVBoxLayout(inner);
    lay->setContentsMargins(40, 40, 40, 40);
    lay->setSpacing(16);

    auto *header = new QLabel("Interactive Basics Tutorial");
    header->setStyleSheet(QString(
        "font-size: 24px; font-weight: 700; color: %1;"
    ).arg(IB_ACCENT));
    lay->addWidget(header);

    auto *desc = new QLabel(
        "A 15-20 minute hands-on tutorial. You'll learn the fundamental concepts of "
        "programming through short explanations and exercises. Pick which languages "
        "you want examples in — more languages = more comparisons."
    );
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("font-size: 13px; color: %1;").arg(IB_FG2));
    lay->addWidget(desc);

    auto *langTitle = new QLabel("What languages do you want to learn about?");
    langTitle->setStyleSheet(QString(
        "font-size: 14px; font-weight: 700; color: %1; margin-top: 8px;"
    ).arg(IB_FG));
    lay->addWidget(langTitle);

    auto makeCheck = [&](const QString &label, bool checked) -> QCheckBox* {
        auto *cb = new QCheckBox(label, inner);
        cb->setChecked(checked);
        cb->setStyleSheet(QString(
            "QCheckBox { color: %1; font-size: 13px; spacing: 8px; background: transparent; }"
            "QCheckBox::indicator { width: 16px; height: 16px; }"
            "QCheckBox::indicator:unchecked { border: 2px solid %2; border-radius: 3px; background: transparent; }"
            "QCheckBox::indicator:checked { border: 2px solid %3; border-radius: 3px; background: %3; }"
        ).arg(IB_FG, IB_BORDER, IB_ACCENT));
        return cb;
    };

    m_chkPython = makeCheck("Python  —  beginner-friendly, easy to read", true);
    m_chkC      = makeCheck("C  —  how memory and types really work", false);
    m_chkRust   = makeCheck("Rust  —  ownership and safety guarantees", false);
    m_chkUL     = makeCheck("UniLogic  —  compile to multiple targets", false);

    lay->addWidget(m_chkPython);
    lay->addWidget(m_chkC);
    lay->addWidget(m_chkRust);
    lay->addWidget(m_chkUL);

    auto *hint = new QLabel("You can select multiple languages. At least one required.");
    hint->setStyleSheet(QString("font-size: 11px; color: %1;").arg(IB_FG3));
    lay->addWidget(hint);

    m_startBtn = new QPushButton("Start Tutorial ->", inner);
    m_startBtn->setFixedHeight(38);
    m_startBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: #1e1e2e; font-size: 13px; font-weight: 700; "
        "border-radius: 6px; border: none; padding: 6px 24px; }"
        "QPushButton:hover { background: #a6c8ff; }"
    ).arg(IB_ACCENT));
    connect(m_startBtn, &QPushButton::clicked, this, &InteractiveBasicsDialog::onStartClicked);
    lay->addWidget(m_startBtn);
    lay->addStretch();

    auto *centreWrap = new QHBoxLayout;
    centreWrap->addStretch();
    centreWrap->addWidget(inner);
    centreWrap->addStretch();
    outerLay->addStretch();
    outerLay->addLayout(centreWrap);
    outerLay->addStretch();

    return outer;
}

// ============================================================================
// Page 1 — Types & Variables
// ============================================================================

QWidget* InteractiveBasicsDialog::buildTopic1Page()
{
    auto *content = new QWidget;
    content->setMaximumWidth(640);
    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(32, 28, 32, 28);
    lay->setSpacing(14);

    lay->addWidget(makeSectionTitle("TOPIC 1 OF 7", "Types & Variables",
        "What data is, and how programs store and name it."));

    lay->addWidget(makeTeachingLabel(
        "A <b>variable</b> is a named container. When you write <code>price = 12.99</code>, "
        "you create a container called <i>price</i> and put the number 12.99 inside it. "
        "You can read it, change it, and use it anywhere later in your code.",
        content));

    lay->addWidget(makeTeachingLabel(
        "<b>Types</b> describe what kind of data a variable holds:<br>"
        "&nbsp;&nbsp;<code>42</code> &rarr; <b>int</b> (whole number)<br>"
        "&nbsp;&nbsp;<code>3.14</code> &rarr; <b>float</b> (decimal)<br>"
        "&nbsp;&nbsp;<code>\"hello\"</code> &rarr; <b>string</b> (text)<br>"
        "&nbsp;&nbsp;<code>True</code> / <code>False</code> &rarr; <b>bool</b> (yes/no)",
        content));

    lay->addWidget(makeCodeBlock(
        "# Python\n"
        "age    = 25        # int\n"
        "score  = 9.8       # float\n"
        "name   = \"Alice\"   # string\n"
        "active = True      # bool\n"
        "print(age, score, name, active)",
        "Python", content));

    lay->addWidget(makeCodeBlock(
        "// C\n"
        "int   age    = 25;\n"
        "float score  = 9.8f;\n"
        "char* name   = \"Alice\";\n"
        "int   active = 1;  /* 1 = true, 0 = false */",
        "C", content));

    // Exercise 1: multiple choice
    auto ex1 = makeRadioExercise(content, lay,
        "What type is <code>x = 3.14</code>?",
        {"int", "float", "string", "bool"},
        1,   // correct = float
        "Correct! <code>3.14</code> has a decimal point so it's a <b>float</b>.");
    connect(ex1.checkBtn, &QPushButton::clicked, this,
        &InteractiveBasicsDialog::onCheckAnswerClicked);
    m_topicExercises[1].exercises.append(ex1);

    // Exercise 2: text answer
    auto ex2 = makeTextExercise(content, lay,
        "If <code>x = 5</code> and <code>y = 3</code>, what does <code>x + y</code> evaluate to?",
        "8",
        "Correct! <code>5 + 3 = 8</code>. Adding two ints always gives an int.");
    connect(ex2.checkBtn, &QPushButton::clicked, this,
        &InteractiveBasicsDialog::onCheckAnswerClicked);
    m_topicExercises[1].exercises.append(ex2);

    // Hide second exercise until first is answered
    ex2.feedbackLabel->parentWidget(); // compiled reference
    m_topicExercises[1].exercises[1].checkBtn->setVisible(false);
    m_topicExercises[1].exercises[1].lineEdit->setVisible(false);
    if (lay->count() > 0) {
        // We'll hide from the outside — find the sep + question + input + feedback + check
    }

    lay->addStretch();

    auto *page = new QWidget;
    auto *pageLay = new QHBoxLayout(page);
    pageLay->setContentsMargins(0,0,0,0);
    pageLay->addStretch();
    pageLay->addWidget(content);
    pageLay->addStretch();

    return wrapInScroll(page);
}

// ============================================================================
// Page 2 — Functions
// ============================================================================

QWidget* InteractiveBasicsDialog::buildTopic2Page()
{
    auto *content = new QWidget;
    content->setMaximumWidth(640);
    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(32, 28, 32, 28);
    lay->setSpacing(14);

    lay->addWidget(makeSectionTitle("TOPIC 2 OF 7", "Functions",
        "Reusable blocks of code you can call by name."));

    lay->addWidget(makeTeachingLabel(
        "A <b>function</b> is a named block of code you write once and call many times. "
        "It takes <b>parameters</b> (inputs) and may <b>return</b> a result (output). "
        "Functions let you avoid copy-pasting the same logic everywhere.",
        content));

    lay->addWidget(makeCodeBlock(
        "# Python\n"
        "def add(a, b):\n"
        "    return a + b\n"
        "\n"
        "result = add(3, 7)   # result = 10\n"
        "print(result)",
        "Python", content));

    lay->addWidget(makeCodeBlock(
        "// C\n"
        "int add(int a, int b) {\n"
        "    return a + b;\n"
        "}\n"
        "\n"
        "int result = add(3, 7);  /* result = 10 */",
        "C", content));

    lay->addWidget(makeTeachingLabel(
        "In Python, you declare functions with <code>def</code>. In C, you must specify "
        "the <b>type</b> of every parameter and the return type. Rust uses <code>fn</code>. "
        "In all cases, the idea is the same: give it a name, list inputs, and return something.",
        content));

    // Exercise 1: multiple choice — what does a function return
    auto ex1 = makeRadioExercise(content, lay,
        "Given this Python function:<br>"
        "<code>def double(n):<br>&nbsp;&nbsp;&nbsp;&nbsp;return n * 2</code><br><br>"
        "What does <code>double(6)</code> return?",
        {"6", "2", "12", "36"},
        2,  // 12
        "Correct! <code>6 * 2 = 12</code>. The function multiplies its input by 2.");
    connect(ex1.checkBtn, &QPushButton::clicked, this,
        &InteractiveBasicsDialog::onCheckAnswerClicked);
    m_topicExercises[2].exercises.append(ex1);

    // Exercise 2: text answer — what does this return
    auto ex2 = makeTextExercise(content, lay,
        "What is the return type of this C function?<br>"
        "<code>float average(float a, float b) { return (a + b) / 2.0f; }</code>",
        "float",
        "Correct! The return type <code>float</code> is declared before the function name in C.");
    connect(ex2.checkBtn, &QPushButton::clicked, this,
        &InteractiveBasicsDialog::onCheckAnswerClicked);
    m_topicExercises[2].exercises.append(ex2);

    m_topicExercises[2].exercises[1].checkBtn->setVisible(false);
    m_topicExercises[2].exercises[1].lineEdit->setVisible(false);

    lay->addStretch();

    auto *page = new QWidget;
    auto *pageLay = new QHBoxLayout(page);
    pageLay->setContentsMargins(0,0,0,0);
    pageLay->addStretch();
    pageLay->addWidget(content);
    pageLay->addStretch();

    return wrapInScroll(page);
}

// ============================================================================
// Page 3 — If / Else Logic
// ============================================================================

QWidget* InteractiveBasicsDialog::buildTopic3Page()
{
    auto *content = new QWidget;
    content->setMaximumWidth(640);
    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(32, 28, 32, 28);
    lay->setSpacing(14);

    lay->addWidget(makeSectionTitle("TOPIC 3 OF 7", "If / Else Logic",
        "Making decisions: running different code based on conditions."));

    lay->addWidget(makeTeachingLabel(
        "An <b>if</b> statement runs a block of code only when a condition is true. "
        "An <b>else</b> provides a fallback. You can chain multiple conditions with "
        "<b>elif</b> (Python) or <b>else if</b> (C/Rust). This is how programs make decisions.",
        content));

    lay->addWidget(makeCodeBlock(
        "# Python\n"
        "x = 7\n"
        "if x > 10:\n"
        "    print('big')\n"
        "elif x > 5:\n"
        "    print('medium')\n"
        "else:\n"
        "    print('small')",
        "Python", content));

    lay->addWidget(makeCodeBlock(
        "// C\n"
        "int x = 7;\n"
        "if (x > 10) {\n"
        "    printf(\"big\\n\");\n"
        "} else if (x > 5) {\n"
        "    printf(\"medium\\n\");\n"
        "} else {\n"
        "    printf(\"small\\n\");\n"
        "}",
        "C", content));

    // Exercise 1: what does the code above output for x=7
    auto ex1 = makeRadioExercise(content, lay,
        "Using the Python code above with <code>x = 7</code>, what gets printed?",
        {"big", "medium", "small", "Nothing"},
        1,  // medium
        "Correct! <code>x = 7</code> is not &gt; 10, but it IS &gt; 5, so the <b>elif</b> branch runs and prints 'medium'.");
    connect(ex1.checkBtn, &QPushButton::clicked, this,
        &InteractiveBasicsDialog::onCheckAnswerClicked);
    m_topicExercises[3].exercises.append(ex1);

    // Exercise 2: what's wrong
    auto ex2 = makeRadioExercise(content, lay,
        "Which of these is a bug in an if/else block?",
        {
            "elif before if",
            "else has no condition — that's correct",
            "Using == to compare equality",
            "Having an else block at all"
        },
        0,  // elif before if
        "Correct! <code>elif</code> and <code>else</code> must come after an <code>if</code>. "
        "You can't start a decision chain with elif.");
    connect(ex2.checkBtn, &QPushButton::clicked, this,
        &InteractiveBasicsDialog::onCheckAnswerClicked);
    m_topicExercises[3].exercises.append(ex2);

    m_topicExercises[3].exercises[1].checkBtn->setVisible(false);
    for (auto *rb : m_topicExercises[3].exercises[1].radios) rb->setVisible(false);

    lay->addStretch();

    auto *page = new QWidget;
    auto *pageLay = new QHBoxLayout(page);
    pageLay->setContentsMargins(0,0,0,0);
    pageLay->addStretch();
    pageLay->addWidget(content);
    pageLay->addStretch();

    return wrapInScroll(page);
}

// ============================================================================
// Page 4 — Loops
// ============================================================================

QWidget* InteractiveBasicsDialog::buildTopic4Page()
{
    auto *content = new QWidget;
    content->setMaximumWidth(640);
    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(32, 28, 32, 28);
    lay->setSpacing(14);

    lay->addWidget(makeSectionTitle("TOPIC 4 OF 7", "Loops",
        "Running the same code multiple times without copy-pasting."));

    lay->addWidget(makeTeachingLabel(
        "A <b>for</b> loop runs a block once for each item in a sequence. "
        "A <b>while</b> loop keeps running as long as a condition is true. "
        "Without loops, processing 1000 items would require 1000 lines of code.",
        content));

    lay->addWidget(makeCodeBlock(
        "# Python — count from 0 to 4\n"
        "for i in range(5):\n"
        "    print(i)\n"
        "# prints: 0 1 2 3 4\n"
        "\n"
        "# while loop\n"
        "count = 0\n"
        "while count < 3:\n"
        "    print(count)\n"
        "    count += 1\n"
        "# prints: 0 1 2",
        "Python", content));

    lay->addWidget(makeCodeBlock(
        "// C — for loop\n"
        "for (int i = 0; i < 5; i++) {\n"
        "    printf(\"%d\\n\", i);\n"
        "}\n"
        "\n"
        "// accumulator pattern\n"
        "int total = 0;\n"
        "for (int i = 1; i <= 4; i++) {\n"
        "    total += i;\n"
        "}\n"
        "/* total = 10 */",
        "C", content));

    // Exercise 1: how many times does range(5) loop
    auto ex1 = makeRadioExercise(content, lay,
        "How many times does <code>for i in range(5):</code> execute the loop body?",
        {"4 times", "5 times", "6 times", "Until stopped"},
        1,  // 5 times
        "Correct! <code>range(5)</code> produces 0, 1, 2, 3, 4 — that's <b>5</b> iterations.");
    connect(ex1.checkBtn, &QPushButton::clicked, this,
        &InteractiveBasicsDialog::onCheckAnswerClicked);
    m_topicExercises[4].exercises.append(ex1);

    // Exercise 2: what does the accumulator print
    auto ex2 = makeTextExercise(content, lay,
        "In the C code, after the accumulator loop runs, what is <code>total</code>?<br>"
        "<small>(adds 1+2+3+4)</small>",
        "10",
        "Correct! 1 + 2 + 3 + 4 = <b>10</b>.");
    connect(ex2.checkBtn, &QPushButton::clicked, this,
        &InteractiveBasicsDialog::onCheckAnswerClicked);
    m_topicExercises[4].exercises.append(ex2);

    m_topicExercises[4].exercises[1].checkBtn->setVisible(false);
    m_topicExercises[4].exercises[1].lineEdit->setVisible(false);

    lay->addStretch();

    auto *page = new QWidget;
    auto *pageLay = new QHBoxLayout(page);
    pageLay->setContentsMargins(0,0,0,0);
    pageLay->addStretch();
    pageLay->addWidget(content);
    pageLay->addStretch();

    return wrapInScroll(page);
}

// ============================================================================
// Page 5 — Collections
// ============================================================================

QWidget* InteractiveBasicsDialog::buildTopic5Page()
{
    auto *content = new QWidget;
    content->setMaximumWidth(640);
    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(32, 28, 32, 28);
    lay->setSpacing(14);

    lay->addWidget(makeSectionTitle("TOPIC 5 OF 7", "Collections (Arrays & Lists)",
        "Storing multiple values together and working through them."));

    lay->addWidget(makeTeachingLabel(
        "A <b>list</b> (Python) or <b>array</b> (C) stores multiple values in order. "
        "Each item has an <b>index</b> starting at 0. "
        "<code>arr[0]</code> is the first item. <code>arr[2]</code> is the third.",
        content));

    lay->addWidget(makeCodeBlock(
        "# Python\n"
        "arr = [10, 20, 30, 40]\n"
        "print(arr[0])   # 10\n"
        "print(arr[2])   # 30\n"
        "\n"
        "# Iterate\n"
        "for item in arr:\n"
        "    print(item)\n"
        "\n"
        "# Append\n"
        "arr.append(50)\n"
        "print(len(arr))  # 5",
        "Python", content));

    lay->addWidget(makeCodeBlock(
        "// C — fixed-size array\n"
        "int arr[] = {10, 20, 30, 40};\n"
        "printf(\"%d\\n\", arr[0]);  /* 10 */\n"
        "printf(\"%d\\n\", arr[2]);  /* 30 */\n"
        "\n"
        "/* Iterate */\n"
        "for (int i = 0; i < 4; i++) {\n"
        "    printf(\"%d\\n\", arr[i]);\n"
        "}",
        "C", content));

    // Exercise 1: index access
    auto ex1 = makeTextExercise(content, lay,
        "Given <code>arr = [10, 20, 30, 40]</code>, what is <code>arr[2]</code>?",
        "30",
        "Correct! Indexing starts at 0, so index 2 is the <b>third</b> element: 30.");
    connect(ex1.checkBtn, &QPushButton::clicked, this,
        &InteractiveBasicsDialog::onCheckAnswerClicked);
    m_topicExercises[5].exercises.append(ex1);

    // Exercise 2: loop output
    auto ex2 = makeRadioExercise(content, lay,
        "What does <code>for item in [1, 2, 3]: print(item * 2)</code> print?",
        {"1 2 3", "2 4 6", "1 4 9", "6"},
        1,  // 2 4 6
        "Correct! The loop doubles each item: 1×2=2, 2×2=4, 3×2=6.");
    connect(ex2.checkBtn, &QPushButton::clicked, this,
        &InteractiveBasicsDialog::onCheckAnswerClicked);
    m_topicExercises[5].exercises.append(ex2);

    m_topicExercises[5].exercises[1].checkBtn->setVisible(false);
    for (auto *rb : m_topicExercises[5].exercises[1].radios) rb->setVisible(false);

    lay->addStretch();

    auto *page = new QWidget;
    auto *pageLay = new QHBoxLayout(page);
    pageLay->setContentsMargins(0,0,0,0);
    pageLay->addStretch();
    pageLay->addWidget(content);
    pageLay->addStretch();

    return wrapInScroll(page);
}

// ============================================================================
// Page 6 — Objects & Inheritance
// ============================================================================

QWidget* InteractiveBasicsDialog::buildTopic6Page()
{
    auto *content = new QWidget;
    content->setMaximumWidth(640);
    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(32, 28, 32, 28);
    lay->setSpacing(14);

    lay->addWidget(makeSectionTitle("TOPIC 6 OF 7", "Objects & Inheritance",
        "Grouping data and behaviour together; building on existing types."));

    // C-only note
    auto *cOnlyNote = new QFrame(content);
    cOnlyNote->setObjectName("cOnlyNote");
    cOnlyNote->setStyleSheet(QString(
        "QFrame { background: #2a2a18; border-radius: 6px; border-left: 3px solid %1; }"
    ).arg(IB_YELLOW));
    {
        auto *nl = new QVBoxLayout(cOnlyNote);
        nl->setContentsMargins(14, 10, 14, 10);
        auto *ntxt = new QLabel(
            "<b>Note:</b> C doesn't have classes — it uses <code>struct</code> (data only). "
            "This topic shows Python and UniLogic examples. If you selected C only, you can "
            "still read along — the concept of grouping data applies everywhere.",
            cOnlyNote);
        ntxt->setWordWrap(true);
        ntxt->setTextFormat(Qt::RichText);
        ntxt->setStyleSheet(QString("font-size: 12px; color: %1;").arg(IB_YELLOW));
        nl->addWidget(ntxt);
    }
    lay->addWidget(cOnlyNote);

    lay->addWidget(makeTeachingLabel(
        "A <b>class</b> is a template that bundles data (fields) and behaviour (methods). "
        "An <b>object</b> is one instance created from the template. "
        "<b>Inheritance</b> lets a subclass (child) extend a parent class, "
        "automatically getting all of the parent's methods.",
        content));

    lay->addWidget(makeCodeBlock(
        "# Python\n"
        "class Animal:\n"
        "    def __init__(self, name):\n"
        "        self.name = name\n"
        "    def speak(self):\n"
        "        return 'Some sound'\n"
        "\n"
        "class Dog(Animal):          # Dog inherits Animal\n"
        "    def speak(self):\n"
        "        return 'Woof!'\n"
        "\n"
        "d = Dog('Rex')\n"
        "print(d.speak())            # Woof!\n"
        "print(d.name)               # Rex",
        "Python", content));

    // Exercise 1: inheritance yes/no
    auto ex1 = makeRadioExercise(content, lay,
        "If <code>Dog</code> inherits <code>Animal</code>, and <code>Animal</code> has a "
        "<code>speak()</code> method, can <code>Dog</code> call <code>speak()</code>?",
        {"Yes — it inherits the method", "No — Dog must define its own speak()"},
        0,  // Yes
        "Correct! Inheritance means Dog automatically gets speak() from Animal — "
        "and can override it with its own version, as shown above.");
    connect(ex1.checkBtn, &QPushButton::clicked, this,
        &InteractiveBasicsDialog::onCheckAnswerClicked);
    m_topicExercises[6].exercises.append(ex1);

    // Exercise 2: what does d.name print
    auto ex2 = makeTextExercise(content, lay,
        "In the Python code above, what does <code>print(d.name)</code> output?",
        "Rex",
        "Correct! <code>Rex</code> was passed to <code>__init__</code> and stored as <code>self.name</code>.");
    connect(ex2.checkBtn, &QPushButton::clicked, this,
        &InteractiveBasicsDialog::onCheckAnswerClicked);
    m_topicExercises[6].exercises.append(ex2);

    m_topicExercises[6].exercises[1].checkBtn->setVisible(false);
    m_topicExercises[6].exercises[1].lineEdit->setVisible(false);

    lay->addStretch();

    auto *page = new QWidget;
    auto *pageLay = new QHBoxLayout(page);
    pageLay->setContentsMargins(0,0,0,0);
    pageLay->addStretch();
    pageLay->addWidget(content);
    pageLay->addStretch();

    return wrapInScroll(page);
}

// ============================================================================
// Page 7 — Language Deep Dive
// ============================================================================

QWidget* InteractiveBasicsDialog::buildTopic7Page()
{
    auto *content = new QWidget;
    content->setMaximumWidth(640);
    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(32, 28, 32, 28);
    lay->setSpacing(14);

    lay->addWidget(makeSectionTitle("TOPIC 7 OF 7", "Language Deep Dive",
        "Unique features that set each language apart."));

    // Python section
    auto *pyTitle = new QLabel("Python specifics");
    pyTitle->setStyleSheet(QString(
        "font-size: 14px; font-weight: 700; color: %1;"
    ).arg(IB_ACCENT));
    lay->addWidget(pyTitle);

    lay->addWidget(makeTeachingLabel(
        "In Python, <b>everything is an object</b> — including integers and functions. "
        "The <code>__name__ == \"__main__\"</code> guard prevents code from running "
        "when you import the file as a module. <code>pip install</code> adds packages.",
        content));

    lay->addWidget(makeCodeBlock(
        "# Python\n"
        "# Guard — only runs when the script is executed directly\n"
        "if __name__ == '__main__':\n"
        "    print('Hello from main!')\n"
        "\n"
        "# Everything is an object\n"
        "x = 42\n"
        "print(type(x))        # <class 'int'>\n"
        "print(x.bit_length()) # 6",
        "Python", content));

    // C section
    auto *cTitle = new QLabel("C specifics");
    cTitle->setStyleSheet(QString(
        "font-size: 14px; font-weight: 700; color: %1;"
    ).arg(IB_ACCENT));
    lay->addWidget(cTitle);

    lay->addWidget(makeTeachingLabel(
        "C gives you direct memory control. <code>sizeof(type)</code> tells you how many "
        "bytes something uses. A <b>pointer</b> stores a memory address. "
        "<code>malloc</code> allocates heap memory; <code>free</code> releases it. "
        "Forgetting to free causes a memory leak.",
        content));

    lay->addWidget(makeCodeBlock(
        "// C — pointers and memory\n"
        "int x = 10;\n"
        "int *p = &x;      /* p holds the address of x */\n"
        "printf(\"%d\\n\", *p); /* dereference: prints 10 */\n"
        "\n"
        "printf(\"%zu\\n\", sizeof(int));  /* 4 bytes on most systems */\n"
        "\n"
        "/* Heap allocation */\n"
        "int *arr = malloc(5 * sizeof(int));\n"
        "arr[0] = 42;\n"
        "free(arr);  /* MUST free when done */",
        "C", content));

    // Rust section
    auto *rustTitle = new QLabel("Rust specifics");
    rustTitle->setStyleSheet(QString(
        "font-size: 14px; font-weight: 700; color: %1;"
    ).arg(IB_ACCENT));
    lay->addWidget(rustTitle);

    lay->addWidget(makeTeachingLabel(
        "Rust enforces <b>ownership</b>: every value has one owner, and when the owner "
        "goes out of scope, the value is dropped. You can <b>borrow</b> a reference "
        "(<code>&amp;x</code>) without taking ownership. "
        "The compiler rejects programs that would cause memory bugs at compile time — "
        "no runtime crashes from use-after-free.",
        content));

    lay->addWidget(makeCodeBlock(
        "// Rust\n"
        "fn main() {\n"
        "    let s1 = String::from(\"hello\");\n"
        "    let s2 = &s1;          // borrow — s1 still owns it\n"
        "    println!(\"{}\", s2);   // hello\n"
        "    println!(\"{}\", s1);   // still valid\n"
        "\n"
        "    // ownership move:\n"
        "    let s3 = s1;           // s1 is moved into s3\n"
        "    // println!(\"{}\", s1); // ERROR: s1 is no longer valid\n"
        "}",
        "Rust", content));

    // UniLogic section
    auto *ulTitle = new QLabel("UniLogic specifics");
    ulTitle->setStyleSheet(QString(
        "font-size: 14px; font-weight: 700; color: %1;"
    ).arg(IB_ACCENT));
    lay->addWidget(ulTitle);

    lay->addWidget(makeTeachingLabel(
        "UniLogic lets you write code once and <b>compile to multiple targets</b>: "
        "Python, C, JavaScript, WebAssembly. You can configure the memory model "
        "(GC, ownership, or manual) and concurrency strategy per target. "
        "This makes it easy to prototype fast and deploy anywhere.",
        content));

    // Exercise: Python __main__ guard
    auto ex1 = makeRadioExercise(content, lay,
        "In Python, why do we use <code>if __name__ == '__main__':</code>?",
        {
            "It speeds up the program",
            "It prevents the code from running when the file is imported as a module",
            "It is required for all Python scripts",
            "It defines the main function like in C"
        },
        1,
        "Correct! When Python imports a file as a module, <code>__name__</code> is set to "
        "the module's name, not <code>'__main__'</code>, so the guarded code is skipped.");
    connect(ex1.checkBtn, &QPushButton::clicked, this,
        &InteractiveBasicsDialog::onCheckAnswerClicked);
    m_topicExercises[7].exercises.append(ex1);

    // Exercise 2: C pointer
    auto ex2 = makeTextExercise(content, lay,
        "In C, if <code>int x = 10;</code> and <code>int *p = &amp;x;</code>, "
        "what does <code>*p</code> evaluate to?",
        "10",
        "Correct! <code>*p</code> dereferences the pointer — it gives you the value at the address, which is 10.");
    connect(ex2.checkBtn, &QPushButton::clicked, this,
        &InteractiveBasicsDialog::onCheckAnswerClicked);
    m_topicExercises[7].exercises.append(ex2);

    m_topicExercises[7].exercises[1].checkBtn->setVisible(false);
    m_topicExercises[7].exercises[1].lineEdit->setVisible(false);

    lay->addStretch();

    auto *page = new QWidget;
    auto *pageLay = new QHBoxLayout(page);
    pageLay->setContentsMargins(0,0,0,0);
    pageLay->addStretch();
    pageLay->addWidget(content);
    pageLay->addStretch();

    return wrapInScroll(page);
}

// ============================================================================
// Page 8 — Done
// ============================================================================

QWidget* InteractiveBasicsDialog::buildDonePage()
{
    auto *outer = new QWidget;
    auto *outerLay = new QVBoxLayout(outer);
    outerLay->setContentsMargins(0,0,0,0);

    auto *inner = new QWidget;
    inner->setMaximumWidth(560);
    auto *lay = new QVBoxLayout(inner);
    lay->setContentsMargins(40, 40, 40, 40);
    lay->setSpacing(16);

    auto *icon = new QLabel("Basics complete!", inner);
    icon->setStyleSheet(QString(
        "font-size: 26px; font-weight: 700; color: %1;"
    ).arg(IB_GREEN));
    lay->addWidget(icon);

    auto *sub = new QLabel(
        "You've worked through all 7 core programming concepts. "
        "Here's a summary of what you now understand:",
        inner);
    sub->setWordWrap(true);
    sub->setStyleSheet(QString("font-size: 13px; color: %1;").arg(IB_FG2));
    lay->addWidget(sub);

    QStringList concepts = {
        "Types & Variables — naming and categorising data",
        "Functions — reusable code blocks with inputs and outputs",
        "If / Else Logic — making decisions based on conditions",
        "Loops — repeating code efficiently",
        "Collections — storing and iterating over multiple values",
        "Objects & Inheritance — bundling data and behaviour",
        "Language Deep Dive — what makes Python, C, Rust, and UniLogic distinctive"
    };

    auto *listFrame = new QFrame(inner);
    listFrame->setStyleSheet(QString(
        "QFrame { background: %1; border-radius: 8px; }"
    ).arg(IB_BG2));
    auto *listLay = new QVBoxLayout(listFrame);
    listLay->setContentsMargins(20, 16, 20, 16);
    listLay->setSpacing(8);

    for (const QString &c : concepts) {
        auto *row = new QHBoxLayout;
        auto *dot = new QLabel("•", listFrame);
        dot->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;").arg(IB_GREEN));
        dot->setFixedWidth(14);
        auto *txt = new QLabel(c, listFrame);
        txt->setWordWrap(true);
        txt->setStyleSheet(QString("font-size: 12px; color: %1;").arg(IB_FG));
        row->addWidget(dot, 0, Qt::AlignTop);
        row->addWidget(txt, 1);
        listLay->addLayout(row);
    }
    lay->addWidget(listFrame);

    auto *nextSteps = new QLabel(
        "<b>What next?</b><br>"
        "Use the <b>Language Guide</b> (Tools menu) for syntax reference.<br>"
        "Open a real file and click <b>Explain</b> to see line-by-line annotations.<br>"
        "Try the <b>AI Chat</b> — ask any question about code you're looking at.",
        inner);
    nextSteps->setWordWrap(true);
    nextSteps->setTextFormat(Qt::RichText);
    nextSteps->setStyleSheet(QString("font-size: 12px; color: %1;").arg(IB_FG2));
    lay->addWidget(nextSteps);

    auto *closeBtn = new QPushButton("Close Tutorial", inner);
    closeBtn->setFixedHeight(36);
    closeBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: #1e1e2e; font-size: 13px; font-weight: 700; "
        "border-radius: 6px; border: none; }"
        "QPushButton:hover { background: #8dd09b; }"
    ).arg(IB_GREEN));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    lay->addWidget(closeBtn);
    lay->addStretch();

    auto *centreWrap = new QHBoxLayout;
    centreWrap->addStretch();
    centreWrap->addWidget(inner);
    centreWrap->addStretch();
    outerLay->addStretch();
    outerLay->addLayout(centreWrap);
    outerLay->addStretch();

    return outer;
}

// ============================================================================
// Navigation
// ============================================================================

void InteractiveBasicsDialog::updateProgress()
{
    m_progressBar->setValue(m_currentStep);
}

void InteractiveBasicsDialog::updateNavButtons()
{
    m_backBtn->setVisible(m_currentStep > 0);

    if (m_currentStep == 0) {
        // Language selection — Next is replaced by Start button
        m_nextBtn->setVisible(false);
    } else if (m_currentStep == TOTAL_STEPS - 1) {
        // Done page
        m_nextBtn->setVisible(false);
    } else {
        m_nextBtn->setVisible(true);
        m_nextBtn->setText("Next ->");
        // Disable Next until exercise is complete
        bool done = m_topicExercises[m_currentStep].topicComplete;
        m_nextBtn->setEnabled(done);
    }
}

void InteractiveBasicsDialog::onStartClicked()
{
    m_wantPython = m_chkPython->isChecked();
    m_wantC      = m_chkC->isChecked();
    m_wantRust   = m_chkRust->isChecked();
    m_wantUL     = m_chkUL->isChecked();

    // Require at least one language
    if (!m_wantPython && !m_wantC && !m_wantRust && !m_wantUL) {
        m_chkPython->setChecked(true);
        m_wantPython = true;
    }

    m_currentStep = 1;
    m_stack->setCurrentIndex(m_currentStep);
    updateProgress();
    updateNavButtons();
}

void InteractiveBasicsDialog::onNextClicked()
{
    if (m_currentStep >= TOTAL_STEPS - 1) return;
    m_currentStep++;
    m_stack->setCurrentIndex(m_currentStep);
    updateProgress();
    updateNavButtons();
}

void InteractiveBasicsDialog::onBackClicked()
{
    if (m_currentStep <= 0) return;
    m_currentStep--;
    m_stack->setCurrentIndex(m_currentStep);
    updateProgress();
    updateNavButtons();
}

// ============================================================================
// Exercise checking
// ============================================================================

void InteractiveBasicsDialog::onCheckAnswerClicked()
{
    // Figure out which step and exercise we're in by matching the sender button
    QPushButton *senderBtn = qobject_cast<QPushButton*>(sender());
    if (!senderBtn) return;

    for (int step = 1; step < TOTAL_STEPS - 1; ++step) {
        auto &topic = m_topicExercises[step];
        for (int exIdx = 0; exIdx < topic.exercises.size(); ++exIdx) {
            auto &ex = topic.exercises[exIdx];
            if (ex.checkBtn != senderBtn) continue;

            bool correct = false;
            QString correctMsg;
            QString wrongMsg;

            if (ex.type == "radio") {
                int sel = ex.radioGroup ? ex.radioGroup->checkedId() : -1;
                correct = (sel == ex.correctRadio);
                correctMsg = ex.feedbackLabel->objectName();
                wrongMsg   = "Not quite — try again. Think carefully about the type.";
            } else if (ex.type == "text") {
                QString answer = ex.lineEdit->text().trimmed().toLower();
                correct = (answer == ex.correctText);
                correctMsg = ex.feedbackLabel->objectName();
                wrongMsg   = QString("Not quite — the answer is <b>%1</b>. Try again.").arg(ex.correctText);
            }

            flashFeedback(ex.feedbackLabel, correct, correctMsg, wrongMsg);

            if (correct) {
                // Disable check button so they can't re-click
                ex.checkBtn->setEnabled(false);
                if (ex.radioGroup) {
                    for (auto *rb : ex.radios) rb->setEnabled(false);
                }
                if (ex.lineEdit) ex.lineEdit->setEnabled(false);

                // Advance to next exercise or mark topic done
                if (exIdx + 1 < topic.exercises.size()) {
                    // Show next exercise
                    auto &next = topic.exercises[exIdx + 1];
                    next.checkBtn->setVisible(true);
                    if (next.type == "text" && next.lineEdit) next.lineEdit->setVisible(true);
                    if (next.type == "radio") {
                        for (auto *rb : next.radios) rb->setVisible(true);
                    }
                } else {
                    // All exercises done — unlock Next
                    topic.topicComplete = true;
                    updateNavButtons();
                }
            }
            return;
        }
    }
}

void InteractiveBasicsDialog::flashFeedback(QLabel *label, bool correct,
                                             const QString &correctMsg,
                                             const QString &wrongMsg)
{
    if (!label) return;

    if (correct) {
        label->setStyleSheet(QString(
            "font-size: 12px; color: %1; background: rgba(166,227,161,0.12); "
            "padding: 6px 10px; border-radius: 5px;"
        ).arg(IB_GREEN));
        label->setText("<b>Correct!</b> " + correctMsg);
    } else {
        label->setStyleSheet(QString(
            "font-size: 12px; color: %1; background: rgba(243,139,168,0.12); "
            "padding: 6px 10px; border-radius: 5px;"
        ).arg(IB_RED));
        label->setText(wrongMsg);
    }
    label->setVisible(true);
}

// ============================================================================
// Utility
// ============================================================================

bool InteractiveBasicsDialog::isCOnly() const
{
    return m_wantC && !m_wantPython && !m_wantRust && !m_wantUL;
}

QString InteractiveBasicsDialog::codeForLanguages(const QMap<QString,QString> &snippets) const
{
    QString result;
    if (m_wantPython && snippets.contains("python")) result += snippets["python"] + "\n\n";
    if (m_wantC      && snippets.contains("c"))      result += snippets["c"]      + "\n\n";
    if (m_wantRust   && snippets.contains("rust"))   result += snippets["rust"]   + "\n\n";
    if (m_wantUL     && snippets.contains("ul"))     result += snippets["ul"]     + "\n\n";
    return result.trimmed();
}
