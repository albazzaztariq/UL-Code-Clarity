#include "core/predictpanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QSettings>
#include <QPropertyAnimation>
#include <QTimer>

// ============================================================================
// PredictPanel
// ============================================================================

PredictPanel::PredictPanel(QWidget* parent)
    : QWidget(parent)
{
    setVisible(false);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    setStyleSheet("background: #1a1a2e; border-bottom: 1px solid #313244;");

    // ── Header row ────────────────────────────────────────────────────────────
    auto* headerWidget = new QWidget;
    headerWidget->setStyleSheet("background: #1a1a2e;");
    auto* headerLayout = new QVBoxLayout(headerWidget);
    headerLayout->setContentsMargins(14, 10, 14, 6);
    headerLayout->setSpacing(6);

    auto* promptLabel = new QLabel("What do you think this will print?");
    promptLabel->setStyleSheet("color: #cdd6f4; font-size: 12px; font-weight: bold;");
    headerLayout->addWidget(promptLabel);

    // Input + buttons row
    auto* inputRow = new QHBoxLayout;
    inputRow->setSpacing(8);

    m_predictionInput = new QLineEdit;
    m_predictionInput->setPlaceholderText("Type your prediction here...");
    m_predictionInput->setFixedHeight(30);
    m_predictionInput->setStyleSheet(
        "QLineEdit { background: #2a2a3c; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; font-family: 'Cascadia Code', 'Consolas', monospace;"
        " font-size: 12px; padding: 0 8px; }"
        "QLineEdit:focus { border-color: #89b4fa; }");
    connect(m_predictionInput, &QLineEdit::returnPressed, this, &PredictPanel::onRunAndCheck);
    inputRow->addWidget(m_predictionInput, 1);

    m_runCheckBtn = new QPushButton("Run & Check");
    m_runCheckBtn->setFixedHeight(30);
    m_runCheckBtn->setCursor(Qt::PointingHandCursor);
    m_runCheckBtn->setStyleSheet(
        "QPushButton { background: #a6e3a1; color: #1e1e2e; font-weight: bold;"
        " font-size: 12px; border-radius: 4px; padding: 0 16px; }"
        "QPushButton:hover { background: #b9f0b4; }"
        "QPushButton:pressed { background: #94d08f; }");
    connect(m_runCheckBtn, &QPushButton::clicked, this, &PredictPanel::onRunAndCheck);
    inputRow->addWidget(m_runCheckBtn);

    headerLayout->addLayout(inputRow);

    // Accuracy row
    m_accuracyLabel = new QLabel;
    m_accuracyLabel->setStyleSheet("color: #6c7086; font-size: 11px;");
    updateAccuracyLabel();
    headerLayout->addWidget(m_accuracyLabel);

    outerLayout->addWidget(headerWidget);

    // ── Result area (hidden until a check is done) ─────────────────────────────
    m_resultArea = new QWidget;
    m_resultArea->setVisible(false);
    m_resultArea->setStyleSheet("background: #1a1a2e;");
    auto* resultLayout = new QVBoxLayout(m_resultArea);
    resultLayout->setContentsMargins(14, 4, 14, 10);
    resultLayout->setSpacing(4);

    auto* sepLine = new QFrame;
    sepLine->setFrameShape(QFrame::HLine);
    sepLine->setStyleSheet("color: #313244;");
    resultLayout->addWidget(sepLine);

    auto* resultIconRow = new QHBoxLayout;
    m_resultIcon = new QLabel;
    m_resultIcon->setStyleSheet("font-size: 14px;");
    resultIconRow->addWidget(m_resultIcon);

    m_resultText = new QLabel;
    m_resultText->setWordWrap(true);
    m_resultText->setStyleSheet("color: #cdd6f4; font-size: 12px;");
    resultIconRow->addWidget(m_resultText, 1);
    resultLayout->addLayout(resultIconRow);

    m_diffArea = new QTextEdit;
    m_diffArea->setReadOnly(true);
    m_diffArea->setVisible(false);
    m_diffArea->setMaximumHeight(80);
    m_diffArea->setStyleSheet(
        "QTextEdit { background: #181825; color: #a6adc8; border: 1px solid #313244;"
        " border-radius: 4px; font-family: 'Cascadia Code', 'Consolas', monospace;"
        " font-size: 11px; }");
    resultLayout->addWidget(m_diffArea);

    outerLayout->addWidget(m_resultArea);
}

// ── Public interface ──────────────────────────────────────────────────────────

void PredictPanel::setActive(bool active)
{
    if (m_active == active) return;
    m_active = active;
    setVisible(active);
    if (!active) {
        reset();
    }
    emit activeChanged(active);
}

void PredictPanel::reset()
{
    if (m_predictionInput) m_predictionInput->clear();
    if (m_resultArea) m_resultArea->setVisible(false);
    if (m_diffArea) m_diffArea->setVisible(false);
    m_waitingForResult = false;
    m_pendingPrediction.clear();
    updateAccuracyLabel();
}

void PredictPanel::checkPrediction(const QString& actualOutput)
{
    if (!m_waitingForResult) return;
    m_waitingForResult = false;

    // Strip trailing whitespace from both sides for comparison
    QString predicted = m_pendingPrediction.trimmed();
    QString actual    = actualOutput.trimmed();

    bool correct = (predicted == actual);

    // Update QSettings accuracy counter
    QSettings s("CodeClarity", "Predict");
    int total   = s.value("accuracy/total", 0).toInt() + 1;
    int correct_ = s.value("accuracy/correct", 0).toInt() + (correct ? 1 : 0);
    s.setValue("accuracy/total", total);
    s.setValue("accuracy/correct", correct_);

    showResult(correct, actualOutput);
    updateAccuracyLabel();
}

void PredictPanel::applyTheme(bool isDark)
{
    Q_UNUSED(isDark);
    // Currently always dark — placeholder for light theme support
}

// ── Private slots ─────────────────────────────────────────────────────────────

void PredictPanel::onRunAndCheck()
{
    m_pendingPrediction = m_predictionInput->text();
    m_waitingForResult  = true;
    m_resultArea->setVisible(false);
    m_diffArea->setVisible(false);
    emit runRequested();
}

// ── Private helpers ───────────────────────────────────────────────────────────

void PredictPanel::showResult(bool correct, const QString& actualOutput)
{
    m_resultArea->setVisible(true);

    if (correct) {
        m_resultArea->setStyleSheet(
            "background: #1a3d28; border-top: 1px solid #40a870;");
        m_resultIcon->setText("✓");
        m_resultIcon->setStyleSheet("color: #a6e3a1; font-size: 16px; font-weight: bold;");
        m_resultText->setText("Correct! You predicted it right.");
        m_resultText->setStyleSheet("color: #a6e3a1; font-size: 12px;");
        m_diffArea->setVisible(false);
    } else {
        m_resultArea->setStyleSheet(
            "background: #3d1f1f; border-top: 1px solid #f38ba8;");
        m_resultIcon->setText("✗");
        m_resultIcon->setStyleSheet("color: #f38ba8; font-size: 16px; font-weight: bold;");

        // Find first line of divergence
        QStringList predictedLines = m_pendingPrediction.split('\n');
        QStringList actualLines    = actualOutput.split('\n');

        int firstDiff = -1;
        int compareLen = qMin(predictedLines.size(), actualLines.size());
        for (int i = 0; i < compareLen; ++i) {
            if (predictedLines[i].trimmed() != actualLines[i].trimmed()) {
                firstDiff = i + 1;
                break;
            }
        }
        if (firstDiff == -1 && predictedLines.size() != actualLines.size())
            firstDiff = compareLen + 1;

        QString explanation;
        if (firstDiff > 0)
            explanation = QString("First difference on line %1.").arg(firstDiff);
        else
            explanation = "Outputs differ in whitespace or formatting.";

        // Truncate actual for display
        QString displayActual = actualOutput.trimmed();
        if (displayActual.length() > 200)
            displayActual = displayActual.left(197) + "...";

        m_resultText->setText(
            QString("Not quite. %1").arg(explanation));
        m_resultText->setStyleSheet("color: #f38ba8; font-size: 12px;");

        // Show diff in monospace area
        m_diffArea->setVisible(true);
        m_diffArea->setHtml(
            QString("<span style='color:#6c7086'>Your prediction:</span> "
                    "<span style='color:#cdd6f4'>%1</span><br>"
                    "<span style='color:#6c7086'>Actual output:</span>   "
                    "<span style='color:#a6e3a1'>%2</span>")
            .arg(m_pendingPrediction.toHtmlEscaped().replace("\n", "<br>"),
                 displayActual.toHtmlEscaped().replace("\n", "<br>")));
    }

    // Flash the result area background briefly
    QTimer::singleShot(400, this, [this, correct]() {
        QString baseColor = correct ? "#1a3d28" : "#3d1f1f";
        m_resultArea->setStyleSheet(
            QString("background: %1; border-top: 1px solid %2;")
            .arg(baseColor, correct ? "#40a870" : "#f38ba8"));
    });
}

void PredictPanel::updateAccuracyLabel()
{
    QSettings s("CodeClarity", "Predict");
    int total   = s.value("accuracy/total", 0).toInt();
    int correct = s.value("accuracy/correct", 0).toInt();

    if (total == 0) {
        m_accuracyLabel->setText("No predictions yet — give it a try!");
    } else {
        int pct = (correct * 100) / total;
        m_accuracyLabel->setText(
            QString("Your prediction accuracy: %1%  (%2/%3)")
            .arg(pct).arg(correct).arg(total));
    }
}
