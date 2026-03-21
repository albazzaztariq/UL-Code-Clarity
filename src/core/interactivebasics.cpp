#include "core/interactivebasics.h"
#include "core/jsonloader.h"
#include "editor/highlighter.h"

#include <QScrollArea>
#include <QScrollBar>
#include <QApplication>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonObject>

// ── Theme ─────────────────────────────────────────────────────────────────────
static const char* IB_BG     = "#1e1e2e";
static const char* IB_BG2    = "#2a2a3c";
static const char* IB_BG3    = "#333348";
static const char* IB_FG     = "#cdd6f4";
static const char* IB_FG2    = "#a6adc8";
static const char* IB_FG3    = "#6c7086";
static const char* IB_ACCENT = "#89b4fa";
static const char* IB_GREEN  = "#a6e3a1";
static const char* IB_RED    = "#f38ba8";
static const char* IB_YELLOW = "#f9e2af";
static const char* IB_BORDER = "#45475a";

// ── Static page helpers ───────────────────────────────────────────────────────

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
    stepLbl->setStyleSheet(QString("font-size: 10px; font-weight: 700; color: %1; letter-spacing: 1px;").arg(IB_FG3));
    lay->addWidget(stepLbl);

    auto *titleLbl = new QLabel(title);
    titleLbl->setStyleSheet(QString("font-size: 20px; font-weight: 700; color: %1;").arg(IB_ACCENT));
    lay->addWidget(titleLbl);

    auto *subLbl = new QLabel(subtitle);
    subLbl->setWordWrap(true);
    subLbl->setStyleSheet(QString("font-size: 12px; color: %1;").arg(IB_FG3));
    lay->addWidget(subLbl);

    return w;
}

// ── Constructor ───────────────────────────────────────────────────────────────

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

    m_topicExercises.resize(TOTAL_STEPS);
    buildUI();
    updateProgress();
    updateNavButtons();
}

// ── UI Construction ───────────────────────────────────────────────────────────

void InteractiveBasicsDialog::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

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

    m_stack = new QStackedWidget(this);
    m_stack->setStyleSheet("background: transparent;");

    QJsonArray topics = JsonLoader::loadArray("interactive_basics.json", "topics");

    m_stack->addWidget(buildLangPage());
    for (int i = 0; i < topics.size(); ++i)
        m_stack->addWidget(buildTopicPage(topics[i].toObject(), i + 1));
    m_stack->addWidget(buildDonePage());

    root->addWidget(m_stack, 1);
    buildNavBar(root);
}

void InteractiveBasicsDialog::buildNavBar(QVBoxLayout *mainLayout)
{
    auto *bar = new QWidget(this);
    bar->setFixedHeight(52);
    bar->setStyleSheet(QString("QWidget { background: %1; border-top: 1px solid %2; }").arg(IB_BG2, IB_BORDER));

    auto *lay = new QHBoxLayout(bar);
    lay->setContentsMargins(20, 0, 20, 0);

    m_backBtn = new QPushButton("<- Back", bar);
    m_backBtn->setFixedHeight(32);
    m_backBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; padding: 6px 20px; font-size: 12px; border-radius: 6px; border: none; }"
        "QPushButton:hover { background: %3; }"
    ).arg(IB_BG3, IB_FG, IB_BORDER));
    connect(m_backBtn, &QPushButton::clicked, this, &InteractiveBasicsDialog::onBackClicked);

    m_nextBtn = new QPushButton("Next ->", bar);
    m_nextBtn->setFixedHeight(32);
    m_nextBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: #1e1e2e; padding: 6px 20px; font-size: 12px; font-weight: 700; border-radius: 6px; border: none; }"
        "QPushButton:disabled { background: %2; color: %3; }"
        "QPushButton:hover:!disabled { background: #a6c8ff; }"
    ).arg(IB_ACCENT, IB_BG3, IB_FG3));
    connect(m_nextBtn, &QPushButton::clicked, this, &InteractiveBasicsDialog::onNextClicked);

    lay->addWidget(m_backBtn);
    lay->addStretch();
    lay->addWidget(m_nextBtn);
    mainLayout->addWidget(bar);
}

// ── Widget factories ──────────────────────────────────────────────────────────

QLabel* InteractiveBasicsDialog::makeTeachingLabel(const QString &html, QWidget *parent)
{
    auto *lbl = new QLabel(html, parent);
    lbl->setWordWrap(true);
    lbl->setTextFormat(Qt::RichText);
    lbl->setStyleSheet(QString("font-size: 13px; color: %1; line-height: 1.7; background: transparent;").arg(IB_FG2));
    lbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    return lbl;
}

QWidget* InteractiveBasicsDialog::makeCodeBlock(const QString &code, const QString &lang, QWidget *parent)
{
    auto *frame = new QFrame(parent);
    frame->setStyleSheet(QString("QFrame { background: #12121c; border-radius: 6px; border-left: 3px solid %1; }").arg(IB_ACCENT));

    auto *lay = new QVBoxLayout(frame);
    lay->setContentsMargins(14, 10, 14, 10);
    lay->setSpacing(4);

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

    auto *hl = new SyntaxHighlighter(editor->document());
    hl->setLanguage(lang);
    Q_UNUSED(hl);

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
        "QPushButton { background: %1; color: #1e1e2e; padding: 4px 18px; font-size: 12px; font-weight: 700; border-radius: 5px; border: none; }"
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

// ── Exercise factories ────────────────────────────────────────────────────────

InteractiveBasicsDialog::ExerciseWidgets InteractiveBasicsDialog::makeRadioExercise(
    QWidget *parent, QVBoxLayout *layout,
    const QString &question, const QStringList &options,
    int correctIndex, const QString &explanation)
{
    ExerciseWidgets ex;
    ex.type = "radio";
    ex.correctRadio = correctIndex;

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
            "QRadioButton::indicator:checked   { border: 2px solid %3; border-radius: 7px; background: %3; }"
        ).arg(IB_FG2, IB_BORDER, IB_ACCENT));
        ex.radioGroup->addButton(rb, i);
        ex.radios.append(rb);
        layout->addWidget(rb);
    }

    ex.feedbackLabel = makeFeedbackLabel(parent);
    ex.feedbackLabel->setObjectName(explanation);
    layout->addWidget(ex.feedbackLabel);

    ex.checkBtn = makeCheckButton(parent);
    layout->addWidget(ex.checkBtn);
    return ex;
}

InteractiveBasicsDialog::ExerciseWidgets InteractiveBasicsDialog::makeTextExercise(
    QWidget *parent, QVBoxLayout *layout,
    const QString &question, const QString &correct, const QString &explanation)
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
        "QLineEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 5px; padding: 5px 10px; font-size: 12px; }"
        "QLineEdit:focus { border-color: %4; }"
    ).arg(IB_BG3, IB_FG, IB_BORDER, IB_ACCENT));
    layout->addWidget(ex.lineEdit);

    ex.feedbackLabel = makeFeedbackLabel(parent);
    ex.feedbackLabel->setObjectName(explanation);
    layout->addWidget(ex.feedbackLabel);

    ex.checkBtn = makeCheckButton(parent);
    layout->addWidget(ex.checkBtn);
    return ex;
}

// ── Page builders ─────────────────────────────────────────────────────────────

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
    header->setStyleSheet(QString("font-size: 24px; font-weight: 700; color: %1;").arg(IB_ACCENT));
    lay->addWidget(header);

    auto *desc = new QLabel(
        "A 15-20 minute hands-on tutorial. You'll learn the fundamental concepts of "
        "programming through short explanations and exercises. Pick which languages "
        "you want examples in — more languages = more comparisons.");
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("font-size: 13px; color: %1;").arg(IB_FG2));
    lay->addWidget(desc);

    auto *langTitle = new QLabel("What languages do you want to learn about?");
    langTitle->setStyleSheet(QString("font-size: 14px; font-weight: 700; color: %1; margin-top: 8px;").arg(IB_FG));
    lay->addWidget(langTitle);

    auto makeCheck = [&](const QString &label, bool checked) -> QCheckBox* {
        auto *cb = new QCheckBox(label, inner);
        cb->setChecked(checked);
        cb->setStyleSheet(QString(
            "QCheckBox { color: %1; font-size: 13px; spacing: 8px; background: transparent; }"
            "QCheckBox::indicator { width: 16px; height: 16px; }"
            "QCheckBox::indicator:unchecked { border: 2px solid %2; border-radius: 3px; background: transparent; }"
            "QCheckBox::indicator:checked   { border: 2px solid %3; border-radius: 3px; background: %3; }"
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
        "QPushButton { background: %1; color: #1e1e2e; font-size: 13px; font-weight: 700; border-radius: 6px; border: none; padding: 6px 24px; }"
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

QWidget* InteractiveBasicsDialog::buildTopicPage(const QJsonObject &topic, int stepIndex)
{
    auto *content = new QWidget;
    content->setMaximumWidth(640);
    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(32, 28, 32, 28);
    lay->setSpacing(14);

    lay->addWidget(makeSectionTitle(
        topic["step_label"].toString(),
        topic["title"].toString(),
        topic["subtitle"].toString()
    ));

    // C-doesn't-have-classes notice for topic 6
    if (topic["id"].toInt() == 6) {
        auto *note = new QFrame(content);
        note->setStyleSheet(QString("QFrame { background: #2a2a18; border-radius: 6px; border-left: 3px solid %1; }").arg(IB_YELLOW));
        auto *nl = new QVBoxLayout(note);
        nl->setContentsMargins(14, 10, 14, 10);
        auto *ntxt = new QLabel(
            "<b>Note:</b> C doesn't have classes — it uses <code>struct</code> (data only). "
            "This topic shows Python/Rust/UniLogic examples. The concept of grouping data still applies.", note);
        ntxt->setWordWrap(true);
        ntxt->setTextFormat(Qt::RichText);
        ntxt->setStyleSheet(QString("font-size: 12px; color: %1;").arg(IB_YELLOW));
        nl->addWidget(ntxt);
        lay->addWidget(note);
    }

    for (const QJsonValue &tv : topic["teaching"].toArray())
        lay->addWidget(makeTeachingLabel(tv.toString(), content));

    for (const QJsonValue &bv : topic["code_blocks"].toArray()) {
        QJsonObject b = bv.toObject();
        lay->addWidget(makeCodeBlock(b["code"].toString(), b["lang"].toString(), content));
    }

    QJsonArray exercises = topic["exercises"].toArray();
    for (int i = 0; i < exercises.size(); ++i) {
        QJsonObject ex = exercises[i].toObject();
        QString type = ex["type"].toString();
        ExerciseWidgets ew;

        if (type == "radio") {
            QStringList opts;
            for (const QJsonValue &ov : ex["options"].toArray())
                opts << ov.toString();
            ew = makeRadioExercise(content, lay,
                ex["question"].toString(), opts,
                ex["correct_index"].toInt(), ex["explanation"].toString());
        } else if (type == "text") {
            ew = makeTextExercise(content, lay,
                ex["question"].toString(),
                ex["correct"].toString(),
                ex["explanation"].toString());
        }

        connect(ew.checkBtn, &QPushButton::clicked, this, &InteractiveBasicsDialog::onCheckAnswerClicked);
        m_topicExercises[stepIndex].exercises.append(ew);

        if (i > 0) {   // hide later exercises until prior ones are answered
            ew.checkBtn->setVisible(false);
            if (ew.lineEdit) ew.lineEdit->setVisible(false);
            for (auto *rb : ew.radios) rb->setVisible(false);
        }
    }

    lay->addStretch();

    auto *page = new QWidget;
    auto *pageLay = new QHBoxLayout(page);
    pageLay->setContentsMargins(0, 0, 0, 0);
    pageLay->addStretch();
    pageLay->addWidget(content);
    pageLay->addStretch();
    return wrapInScroll(page);
}

QWidget* InteractiveBasicsDialog::buildDonePage()
{
    auto *outer = new QWidget;
    auto *outerLay = new QVBoxLayout(outer);
    outerLay->setContentsMargins(0, 0, 0, 0);

    auto *inner = new QWidget;
    inner->setMaximumWidth(560);
    auto *lay = new QVBoxLayout(inner);
    lay->setContentsMargins(40, 40, 40, 40);
    lay->setSpacing(16);

    auto *icon = new QLabel("Basics complete!", inner);
    icon->setStyleSheet(QString("font-size: 26px; font-weight: 700; color: %1;").arg(IB_GREEN));
    lay->addWidget(icon);

    auto *sub = new QLabel(
        "You've worked through all 7 core programming concepts. Here's what you now understand:", inner);
    sub->setWordWrap(true);
    sub->setStyleSheet(QString("font-size: 13px; color: %1;").arg(IB_FG2));
    lay->addWidget(sub);

    static const QStringList concepts = {
        "Types & Variables — naming and categorising data",
        "Functions — reusable code blocks with inputs and outputs",
        "If / Else Logic — making decisions based on conditions",
        "Loops — repeating code efficiently",
        "Collections — storing and iterating over multiple values",
        "Objects & Inheritance — bundling data and behaviour",
        "Language Deep Dive — what makes Python, C, Rust, and UniLogic distinctive"
    };

    auto *listFrame = new QFrame(inner);
    listFrame->setStyleSheet(QString("QFrame { background: %1; border-radius: 8px; }").arg(IB_BG2));
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
        "Try the <b>AI Chat</b> — ask any question about code you're looking at.", inner);
    nextSteps->setWordWrap(true);
    nextSteps->setTextFormat(Qt::RichText);
    nextSteps->setStyleSheet(QString("font-size: 12px; color: %1;").arg(IB_FG2));
    lay->addWidget(nextSteps);

    auto *closeBtn = new QPushButton("Close Tutorial", inner);
    closeBtn->setFixedHeight(36);
    closeBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: #1e1e2e; font-size: 13px; font-weight: 700; border-radius: 6px; border: none; }"
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

// ── Navigation ────────────────────────────────────────────────────────────────

void InteractiveBasicsDialog::updateProgress()   { m_progressBar->setValue(m_currentStep); }

void InteractiveBasicsDialog::updateNavButtons()
{
    m_backBtn->setVisible(m_currentStep > 0);

    if (m_currentStep == 0 || m_currentStep == TOTAL_STEPS - 1) {
        m_nextBtn->setVisible(false);
    } else {
        m_nextBtn->setVisible(true);
        m_nextBtn->setText("Next ->");
        m_nextBtn->setEnabled(m_topicExercises[m_currentStep].topicComplete);
    }
}

void InteractiveBasicsDialog::onStartClicked()
{
    m_wantPython = m_chkPython->isChecked();
    m_wantC      = m_chkC->isChecked();
    m_wantRust   = m_chkRust->isChecked();
    m_wantUL     = m_chkUL->isChecked();

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

// ── Exercise checking ─────────────────────────────────────────────────────────

void InteractiveBasicsDialog::onCheckAnswerClicked()
{
    QPushButton *senderBtn = qobject_cast<QPushButton*>(sender());
    if (!senderBtn) return;

    for (int step = 1; step < TOTAL_STEPS - 1; ++step) {
        auto &topic = m_topicExercises[step];
        for (int exIdx = 0; exIdx < topic.exercises.size(); ++exIdx) {
            auto &ex = topic.exercises[exIdx];
            if (ex.checkBtn != senderBtn) continue;

            bool correct = false;
            QString correctMsg, wrongMsg;

            if (ex.type == "radio") {
                int sel = ex.radioGroup ? ex.radioGroup->checkedId() : -1;
                correct    = (sel == ex.correctRadio);
                correctMsg = ex.feedbackLabel->objectName();
                wrongMsg   = "Not quite — try again. Think carefully about the type.";
            } else if (ex.type == "text") {
                QString answer = ex.lineEdit->text().trimmed().toLower();
                correct    = (answer == ex.correctText);
                correctMsg = ex.feedbackLabel->objectName();
                wrongMsg   = QString("Not quite — the answer is <b>%1</b>. Try again.").arg(ex.correctText);
            }

            flashFeedback(ex.feedbackLabel, correct, correctMsg, wrongMsg);

            if (correct) {
                ex.checkBtn->setEnabled(false);
                if (ex.radioGroup) for (auto *rb : ex.radios) rb->setEnabled(false);
                if (ex.lineEdit)   ex.lineEdit->setEnabled(false);

                if (exIdx + 1 < topic.exercises.size()) {
                    auto &next = topic.exercises[exIdx + 1];
                    next.checkBtn->setVisible(true);
                    if (next.type == "text"  && next.lineEdit) next.lineEdit->setVisible(true);
                    if (next.type == "radio") for (auto *rb : next.radios) rb->setVisible(true);
                } else {
                    topic.topicComplete = true;
                    updateNavButtons();
                }
            }
            return;
        }
    }
}

void InteractiveBasicsDialog::flashFeedback(QLabel *label, bool correct,
                                             const QString &correctMsg, const QString &wrongMsg)
{
    if (!label) return;
    if (correct) {
        label->setStyleSheet(QString(
            "font-size: 12px; color: %1; background: rgba(166,227,161,0.12); padding: 6px 10px; border-radius: 5px;"
        ).arg(IB_GREEN));
        label->setText("<b>Correct!</b> " + correctMsg);
    } else {
        label->setStyleSheet(QString(
            "font-size: 12px; color: %1; background: rgba(243,139,168,0.12); padding: 6px 10px; border-radius: 5px;"
        ).arg(IB_RED));
        label->setText(wrongMsg);
    }
    label->setVisible(true);
}
