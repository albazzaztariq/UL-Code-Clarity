#include "core/rubberduck.h"
#include "core/theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QMessageBox>

// ─── RubberDuckDialog ────────────────────────────────────────────────────────

RubberDuckDialog::RubberDuckDialog(const QString& originalMessage, QWidget* parent)
    : QDialog(parent)
    , m_originalMessage(originalMessage)
{
    setWindowTitle("Rubber Duck Mode");
    setMinimumWidth(480);
    setModal(true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    // Header
    auto* headerLbl = new QLabel("Before I help you, let's think through this together.");
    headerLbl->setWordWrap(true);
    headerLbl->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;").arg(Theme::Colors::accent()));
    root->addWidget(headerLbl);

    auto* subLbl = new QLabel(
        "Answering these questions often reveals the bug before you even send the message.");
    subLbl->setWordWrap(true);
    subLbl->setStyleSheet(QString("color: %1; font-size: 12px;").arg(Theme::Colors::fg2()));
    root->addWidget(subLbl);

    // Separator
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #313244;");  // surface0, no Colors method
    root->addWidget(sep);

    // Field helper
    auto addField = [&](const QString& label, QTextEdit*& field) {
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold;").arg(Theme::Colors::fg()));
        root->addWidget(lbl);
        field = new QTextEdit;
        field->setPlaceholderText("Type here...");
        field->setFixedHeight(72);
        field->setStyleSheet(QString(
            "QTextEdit { background: %1; color: %2; border: 1px solid %3;"
            " border-radius: 4px; padding: 6px; font-size: 12px; }")
            .arg(Theme::Colors::bg2(), Theme::Colors::fg(), Theme::Colors::border()));
        root->addWidget(field);
    };

    addField("1.  What did you expect to happen?", m_expectField);
    addField("2.  What actually happened?",        m_actualField);
    addField("3.  What have you tried so far?",    m_triedField);

    // Buttons
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(10);

    m_solvedBtn = new QPushButton("I figured it out!");
    m_solvedBtn->setCursor(Qt::PointingHandCursor);
    m_solvedBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border-radius: 6px;"
        " padding: 8px 18px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: #c0efbb; }")
        .arg(Theme::Colors::green(), Theme::Colors::bg()));
    btnRow->addWidget(m_solvedBtn);

    btnRow->addStretch();

    m_sendBtn = new QPushButton("Send to AI");
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    m_sendBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border-radius: 6px;"
        " padding: 8px 18px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: #b4d0ff; }")
        .arg(Theme::Colors::accent(), Theme::Colors::bg()));
    btnRow->addWidget(m_sendBtn);

    root->addLayout(btnRow);

    connect(m_sendBtn,   &QPushButton::clicked, this, &RubberDuckDialog::onSendClicked);
    connect(m_solvedBtn, &QPushButton::clicked, this, &RubberDuckDialog::onSolvedClicked);
}

void RubberDuckDialog::onSendClicked()
{
    QString expect = m_expectField->toPlainText().trimmed();
    QString actual = m_actualField->toPlainText().trimmed();
    QString tried  = m_triedField->toPlainText().trimmed();

    QString context;
    if (!expect.isEmpty())
        context += "What I expected: " + expect + "\n";
    if (!actual.isEmpty())
        context += "What actually happened: " + actual + "\n";
    if (!tried.isEmpty())
        context += "What I've tried: " + tried + "\n";

    QString fullMessage;
    if (!context.isEmpty())
        fullMessage = "[Context]\n" + context + "\n[Question]\n" + m_originalMessage;
    else
        fullMessage = m_originalMessage;

    emit sendWithContext(fullMessage);
    accept();
}

void RubberDuckDialog::onSolvedClicked()
{
    RubberDuckToggle::incrementSelfSolveCount();
    int count = RubberDuckToggle::selfSolveCount();

    QString msg = count == 1
        ? "Great! You solved it yourself just by explaining the problem."
        : QString("You've solved it yourself %1 times just by explaining the problem!").arg(count);

    QMessageBox::information(this, "Nice work!", msg);
    emit solvedSelf();
    accept();
}



// ─── RubberDuckToggle ────────────────────────────────────────────────────────

RubberDuckToggle::RubberDuckToggle(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(4, 2, 4, 2);
    lay->setSpacing(4);

    m_check = new QCheckBox(this);
    m_check->setToolTip("Rubber Duck Mode: think before you ask");
    lay->addWidget(m_check);

    m_label = new QLabel("Rubber Duck", this);
    m_label->setStyleSheet(QString("color: %1; font-size: 11px;").arg(Theme::Colors::yellow()));
    m_label->setToolTip("Rubber Duck Mode: think before you ask");
    lay->addWidget(m_label);

    updateSolveLabel();

    connect(m_check, &QCheckBox::toggled, this, [this](bool on) {
        emit toggled(on);
    });
}

bool RubberDuckToggle::isEnabled() const
{
    return m_check && m_check->isChecked();
}

void RubberDuckToggle::incrementSelfSolveCount()
{
    QSettings s("CodeClarity", "RubberDuck");
    int count = s.value("self_solve_count", 0).toInt();
    s.setValue("self_solve_count", count + 1);
}

int RubberDuckToggle::selfSolveCount()
{
    QSettings s("CodeClarity", "RubberDuck");
    return s.value("self_solve_count", 0).toInt();
}

void RubberDuckToggle::updateSolveLabel()
{
    int count = selfSolveCount();
    if (m_label) {
        if (count > 0) {
            m_label->setToolTip(
                QString("You've solved it yourself %1 time%2 by just explaining the problem!")
                .arg(count)
                .arg(count == 1 ? "" : "s"));
        } else {
            m_label->setToolTip("Rubber Duck Mode: think before you ask");
        }
    }
}

void RubberDuckDialog::applyTheme(bool /*isDark*/) {}
void RubberDuckToggle::applyTheme(bool /*isDark*/) {}
