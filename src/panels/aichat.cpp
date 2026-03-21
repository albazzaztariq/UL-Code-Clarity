#include "panels/aichat.h"
#include "panels/modelconfig.h"
#include "core/settingspanel.h"
#include "core/setupwizard.h"
#include <QScrollBar>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>
#include <QTimer>
#include <QMessageBox>

// ============================================================================
// Theme colors
// ============================================================================
static const char* BG      = "#1e1e2e";
static const char* BG2     = "#2a2a3c";
static const char* BG3     = "#333348";
static const char* FG      = "#cdd6f4";
static const char* FG2     = "#a6adc8";
static const char* FG3     = "#6c7086";
static const char* ACCENT  = "#89b4fa";
static const char* BORDER  = "#45475a";

// ============================================================================
// ChatBubble
// ============================================================================

ChatBubble::ChatBubble(Role role, const QString &text, QWidget *parent)
    : QFrame(parent), m_role(role)
{
    setFrameShape(QFrame::NoFrame);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *bubble = new QLabel(text, this);
    bubble->setWordWrap(true);
    bubble->setTextFormat(Qt::RichText);
    bubble->setMaximumWidth(280); // ~85% of 340px panel

    if (role == User) {
        bubble->setStyleSheet(QString(
            "background: %1; color: #1e1e2e; padding: 7px 12px; "
            "border-radius: 12px 12px 2px 12px; font-size: 11px; line-height: 1.4;"
        ).arg(ACCENT));
        layout->addStretch();
        layout->addWidget(bubble);
    } else {
        bubble->setStyleSheet(QString(
            "background: %1; color: %2; padding: 7px 12px; "
            "border-radius: 12px 12px 12px 2px; font-size: 11px; line-height: 1.4;"
            " border: 1px solid %3;"
        ).arg(BG3, FG, BORDER));
        layout->addWidget(bubble);
        layout->addStretch();
    }
}

// ============================================================================
// AIChatPanel
// ============================================================================

AIChatPanel::AIChatPanel(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet(QString("background: %1;").arg(BG2));
    setMinimumHeight(180);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Column header bar: "AI Chat" title + X close button
    m_columnHeader = new QWidget(this);
    m_columnHeader->setStyleSheet(QString(
        "background: %1; border-bottom: 1px solid %2;"
    ).arg(BG2, BORDER));
    auto *headerBarLayout = new QHBoxLayout(m_columnHeader);
    headerBarLayout->setContentsMargins(12, 8, 12, 8);

    m_titleLabel = new QLabel("AI Chat", m_columnHeader);
    m_titleLabel->setStyleSheet(QString(
        "font-size: 12px; font-weight: 700; color: %1; letter-spacing: 0.3px;"
    ).arg(ACCENT));
    headerBarLayout->addWidget(m_titleLabel);
    headerBarLayout->addStretch();

    m_closeButton = new QPushButton(QString::fromUtf8("\xe2\x9c\x95"), m_columnHeader);
    m_closeButton->setFixedSize(16, 16);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setStyleSheet(QString(
        "QPushButton { background: none; color: %1; font-size: 12px; border: none; }"
        "QPushButton:hover { color: %2; }"
    ).arg(FG3, FG));
    connect(m_closeButton, &QPushButton::clicked, this, &AIChatPanel::closeRequested);
    headerBarLayout->addWidget(m_closeButton);

    mainLayout->addWidget(m_columnHeader);

    // No-model warning banner -- shown when no AI provider is configured
    m_noModelBanner = new QWidget(this);
    m_noModelBanner->setStyleSheet(
        "background: #3a2a1a; border-bottom: 1px solid #7c5c2e;");
    auto *bannerLayout = new QHBoxLayout(m_noModelBanner);
    bannerLayout->setContentsMargins(10, 6, 10, 6);
    bannerLayout->setSpacing(8);
    auto *bannerIcon = new QLabel(QString::fromUtf8("\xe2\x9a\xa0"), m_noModelBanner);  // warning
    bannerIcon->setStyleSheet("color: #e8a83a; font-size: 13px;");
    bannerLayout->addWidget(bannerIcon);
    auto *bannerMsg = new QLabel(
        "No AI model configured. AI features disabled.",
        m_noModelBanner);
    bannerMsg->setStyleSheet("color: #d4a96a; font-size: 10px;");
    bannerMsg->setWordWrap(true);
    bannerLayout->addWidget(bannerMsg, 1);
    auto *bannerBtn = new QPushButton("Configure Now", m_noModelBanner);
    bannerBtn->setCursor(Qt::PointingHandCursor);
    bannerBtn->setStyleSheet(
        "QPushButton { background: #5a3e1e; color: #e8c080; border: 1px solid #7c5c2e;"
        " border-radius: 3px; padding: 3px 10px; font-size: 10px; }"
        "QPushButton:hover { background: #7a5428; }");
    connect(bannerBtn, &QPushButton::clicked, this, [this]() {
        SetupWizard *wiz = new SetupWizard(this);
        wiz->setAttribute(Qt::WA_DeleteOnClose);
        connect(wiz, &QDialog::finished, this, [this]() { reloadModelFromConfig(); });
        wiz->show();
    });
    bannerLayout->addWidget(bannerBtn);
    m_noModelBanner->setVisible(false);
    mainLayout->addWidget(m_noModelBanner);

    // Chat content area with padding
    auto *contentWidget = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(8, 8, 8, 8);
    contentLayout->setSpacing(4);

    // Model selector row with gear config button
    auto *headerRow = new QHBoxLayout;
    m_headerLabel = new QLabel("AI CHAT", contentWidget);
    m_headerLabel->setStyleSheet(QString(
        "font-size: 10px; font-weight: 700; text-transform: uppercase; "
        "letter-spacing: 1px; color: %1;"
    ).arg(FG2));
    headerRow->addWidget(m_headerLabel);
    headerRow->addStretch();

    // Model name label populated from saved config — no hardcoded models
    m_modelSelector = new QComboBox(contentWidget);
    m_modelSelector->setStyleSheet(QString(
        "QComboBox { font-size: 10px; background: %1; color: %2; "
        "border: 1px solid %3; border-radius: 4px; padding: 2px 6px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(BG3, FG, BORDER));
    connect(m_modelSelector, &QComboBox::currentTextChanged,
            this, &AIChatPanel::modelChanged);
    headerRow->addWidget(m_modelSelector);

    // Gear button to open model config dialog
    m_configButton = new QPushButton(QString::fromUtf8("\xe2\x9a\x99"), contentWidget);
    m_configButton->setFixedSize(22, 22);
    m_configButton->setCursor(Qt::PointingHandCursor);
    m_configButton->setToolTip("Configure AI model");
    m_configButton->setStyleSheet(QString(
        "QPushButton { background: none; color: %1; font-size: 13px; border: none; }"
        "QPushButton:hover { color: %2; }"
    ).arg(FG3, FG));
    connect(m_configButton, &QPushButton::clicked, this, &AIChatPanel::onConfigClicked);
    headerRow->addWidget(m_configButton);

    contentLayout->addLayout(headerRow);

    // Messages scroll area
    m_scrollArea = new QScrollArea(contentWidget);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 7px; background: transparent; }"
        "QScrollBar::handle:vertical { background: #3c3c54; border-radius: 4px; }"
    );

    m_messagesContainer = new QWidget;
    m_messagesLayout = new QVBoxLayout(m_messagesContainer);
    m_messagesLayout->setContentsMargins(4, 4, 4, 4);
    m_messagesLayout->setSpacing(10);

    // Placeholder shown when chat is empty
    m_placeholderLabel = new QLabel(
        "Ask the AI anything, or use Build from Scratch to start a guided project",
        m_messagesContainer);
    m_placeholderLabel->setWordWrap(true);
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    m_placeholderLabel->setStyleSheet(QString(
        "color: %1; font-size: 11px; font-style: italic; padding: 20px;"
    ).arg(FG2));

    m_messagesLayout->addStretch();
    m_messagesLayout->addWidget(m_placeholderLabel);
    m_messagesLayout->addStretch();

    m_scrollArea->setWidget(m_messagesContainer);
    contentLayout->addWidget(m_scrollArea, 1);

    // Input row
    auto *inputRow = new QHBoxLayout;
    inputRow->setSpacing(4);

    m_input = new QLineEdit(contentWidget);
    m_input->setPlaceholderText("Describe what you want...");
    m_input->setStyleSheet(QString(
        "QLineEdit { padding: 7px 10px; font-size: 11px; background: %1; "
        "color: %2; border: 1px solid %3; border-radius: 4px; }"
    ).arg(BG3, FG, BORDER));
    connect(m_input, &QLineEdit::returnPressed, this, &AIChatPanel::onSendClicked);
    inputRow->addWidget(m_input);

    m_sendButton = new QPushButton("Send", contentWidget);
    m_sendButton->setStyleSheet(QString(
        "QPushButton { background: %1; color: #1e1e2e; font-weight: 600; "
        "padding: 7px 12px; font-size: 11px; border-radius: 4px; }"
        "QPushButton:hover { filter: brightness(1.15); }"
        "QPushButton:disabled { background: %2; color: %3; }"
    ).arg(ACCENT, BG3, FG3));
    connect(m_sendButton, &QPushButton::clicked, this, &AIChatPanel::onSendClicked);
    inputRow->addWidget(m_sendButton);

    // Separator line above input
    auto *sep = new QFrame(contentWidget);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("color: %1;").arg(BORDER));
    contentLayout->addWidget(sep);
    contentLayout->addLayout(inputRow);

    mainLayout->addWidget(contentWidget, 1);

    // Network manager for API calls (kept for backwards compat, no longer primary path)
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &AIChatPanel::onNetworkReply);

    // AI Backend — handles streaming to Anthropic / OpenAI / Ollama
    m_aiBackend = new AIBackend(this);
    connect(m_aiBackend, &AIBackend::responseChunk,   this, &AIChatPanel::onResponseChunk);
    connect(m_aiBackend, &AIBackend::codeBlock,       this, &AIChatPanel::onCodeBlock);
    connect(m_aiBackend, &AIBackend::responseComplete,this, &AIChatPanel::onResponseComplete);
    connect(m_aiBackend, &AIBackend::errorOccurred,   this, &AIChatPanel::onAIError);

    // Load model display from saved settings on startup
    reloadModelFromConfig();

    // Show example messages on first launch only
    QSettings s("CodeClarity", "CodeClarity");
    if (!s.value("hasSeenExamples", false).toBool()) {
        addMessage(ChatBubble::AI,
            "<b>Welcome to Code Clarity!</b><br><br>"
            "This is an example of how the AI Chat works. You can ask questions about your code, "
            "request changes, or get explanations. Try opening a file and asking me to explain it."
        );
        addMessage(ChatBubble::User, "Can you add a function to calculate the total price with tax?");
        addMessage(ChatBubble::AI,
            "Sure! I've added a <code>calculate_total(prices, tax_rate=0.10)</code> function. "
            "It sums the list of prices and multiplies by <code>(1 + tax_rate)</code>. "
            "Check the <b>Clarity</b> panel on the left to see what changed and why."
        );
    }
}

void AIChatPanel::reloadModelFromConfig()
{
    // Load all saved models (validated or not) from SettingsPanel storage
    QList<SavedModel> models = SettingsPanel::loadModels();

    m_modelSelector->blockSignals(true);
    m_modelSelector->clear();

    if (models.isEmpty()) {
        // Fallback: show legacy single-model config
        QSettings s("CodeClarity", "CodeClarity");
        QString provider  = s.value("ai/provider",  "Anthropic").toString();
        QString modelName = s.value("ai/modelName", "").toString();
        QString display   = modelName.isEmpty() ? provider : modelName;
        m_modelSelector->addItem(display);
    } else {
        for (const SavedModel &m : models) {
            QString display = m.modelName.isEmpty() ? m.provider : m.modelName;
            if (m.validated)
                display += " \342\234\224"; // ✔
            m_modelSelector->addItem(display, QVariant::fromValue(models.indexOf(m)));
        }
    }

    m_modelSelector->blockSignals(false);
    checkModelConfigured();
}

void AIChatPanel::checkModelConfigured()
{
    // A model is considered configured if ai/provider is set in QSettings
    // OR any saved model exists in SettingsPanel storage.
    QSettings s("CodeClarity", "CodeClarity");
    bool configured = !s.value("ai/provider", "").toString().isEmpty()
                   || !SettingsPanel::loadModels().isEmpty();
    m_noModelBanner->setVisible(!configured);
    m_sendButton->setEnabled(configured);
    if (!configured)
        m_sendButton->setToolTip("Configure an AI model first (Settings > Models)");
    else
        m_sendButton->setToolTip("");
}

void AIChatPanel::onConfigClicked()
{
    ModelConfigDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        reloadModelFromConfig();
        emit modelChanged(m_modelSelector->currentText());
    }
}

void AIChatPanel::addMessage(ChatBubble::Role role, const QString &text)
{
    // Hide placeholder when the first real message is added
    if (!m_bubbles.isEmpty() == false && m_placeholderLabel->isVisible()) {
        m_placeholderLabel->hide();
    }
    if (m_placeholderLabel->isVisible()) {
        m_placeholderLabel->hide();
    }

    auto *bubble = new ChatBubble(role, text, m_messagesContainer);
    m_bubbles.append(bubble);
    // Insert before the last stretch
    m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, bubble);
    scrollToBottom();
}

void AIChatPanel::clearChat()
{
    for (auto *bubble : m_bubbles) {
        m_messagesLayout->removeWidget(bubble);
        bubble->deleteLater();
    }
    m_bubbles.clear();
    updatePlaceholderVisibility();
}

void AIChatPanel::updatePlaceholderVisibility()
{
    m_placeholderLabel->setVisible(m_bubbles.isEmpty());
}

void AIChatPanel::setBuildFromScratchWelcome()
{
    clearChat();
    addMessage(ChatBubble::AI,
        "<b>Welcome to Build from Scratch!</b><br><br>"
        "Tell me what you want to build. Describe it however you like -- "
        "I'll help you figure out the rest.<br><br>"
        "Once I know what you're making, we'll work through:<br>"
        "<b>1.</b> Which language fits best (and why)<br>"
        "<b>2.</b> What packages/libraries you'll need<br>"
        "<b>3.</b> The overall design -- what functions, what data, what flow<br>"
        "<b>4.</b> Writing the code together, one piece at a time<br><br>"
        "<em>You'll understand every line before we move on.</em>"
    );
    m_input->setPlaceholderText("Describe what you want to build...");
}

void AIChatPanel::setStandardMode()
{
    clearChat();
    addMessage(ChatBubble::AI,
        "Back to standard mode. I'll work on code automatically and explain "
        "what I do in the Clarity panel."
    );
    m_input->setPlaceholderText("Describe what you want...");
}

QString AIChatPanel::selectedModel() const
{
    return m_modelSelector->currentText();
}

void AIChatPanel::markExamplesSeen()
{
    QSettings s("CodeClarity", "CodeClarity");
    if (!s.value("hasSeenExamples", false).toBool()) {
        s.setValue("hasSeenExamples", true);
        clearChat();
    }
}

void AIChatPanel::onSendClicked()
{
    QString text = m_input->text().trimmed();
    if (text.isEmpty()) return;

    // Clear example messages on first real user message
    markExamplesSeen();

    m_input->clear();
    addMessage(ChatBubble::User, text);

    // Disable input while we wait for a response
    m_input->setEnabled(false);
    m_sendButton->setEnabled(false);

    // Emit messageSent — mainwindow checks BFS mode and either routes to BFS
    // or calls m_aiBackend->sendMessage() directly via sendAIMessage().
    emit messageSent(text);
}

void AIChatPanel::setEditorContext(const QString &fileContent, const QString &language)
{
    m_editorContent  = fileContent;
    m_editorLanguage = language;
}

void AIChatPanel::sendAIMessage(const QString &text)
{
    // If multiple models are saved, temporarily write the selected model's
    // config to the ai/* keys so AIBackend picks it up at call time.
    QList<SavedModel> models = SettingsPanel::loadModels();
    int selectedIdx = m_modelSelector->currentIndex();
    if (selectedIdx >= 0 && selectedIdx < models.size()) {
        const SavedModel &m = models[selectedIdx];
        QSettings s("CodeClarity", "CodeClarity");
        s.setValue("ai/provider",  m.provider);
        s.setValue("ai/endpoint",  m.endpoint);
        s.setValue("ai/apiKey",    m.apiKey);
        s.setValue("ai/modelName", m.modelName);
    }
    m_aiBackend->sendMessage(text, m_editorContent, m_editorLanguage, m_assistLevel);
}

void AIChatPanel::setAssistLevel(int level)
{
    m_assistLevel = level;
}

void AIChatPanel::setInputEnabled(bool enabled)
{
    m_input->setEnabled(enabled);
    m_sendButton->setEnabled(enabled);
}

// ── Streaming helpers ───────────────────────────────────────────────────

void AIChatPanel::beginStreamBubble()
{
    // Create a new AI bubble with an empty label we'll update chunk by chunk
    m_streamText.clear();
    m_streamBubble = new ChatBubble(ChatBubble::AI, "", m_messagesContainer);

    // The ChatBubble contains a QLabel — find it
    m_streamLabel = m_streamBubble->findChild<QLabel*>();

    if (m_placeholderLabel->isVisible())
        m_placeholderLabel->hide();

    m_bubbles.append(m_streamBubble);
    m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, m_streamBubble);
    scrollToBottom();
}

void AIChatPanel::appendToStreamBubble(const QString &text)
{
    m_streamText += text;
    if (m_streamLabel)
        m_streamLabel->setText(m_streamText);
    scrollToBottom();
}

void AIChatPanel::finalizeStreamBubble()
{
    m_streamBubble = nullptr;
    m_streamLabel  = nullptr;
    m_streamText.clear();
}

// ── AIBackend signal handlers ───────────────────────────────────────────

void AIChatPanel::onResponseChunk(const QString &text)
{
    if (!m_streamBubble) {
        beginStreamBubble();
    }
    appendToStreamBubble(text);
}

void AIChatPanel::onCodeBlock(const QString &code, const QString &language)
{
    emit codeBlockReceived(code, language);
}

void AIChatPanel::onResponseComplete()
{
    finalizeStreamBubble();
    setInputEnabled(true);
    m_input->setFocus();
}

void AIChatPanel::onAIError(const QString &message)
{
    finalizeStreamBubble();
    addMessage(ChatBubble::AI, "<b>Error:</b> " + message.toHtmlEscaped());
    setInputEnabled(true);
}

void AIChatPanel::onNetworkReply(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        addMessage(ChatBubble::AI,
            QString("Error connecting to API: %1").arg(reply->errorString()));
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isObject()) {
        QJsonObject obj = doc.object();

        // Anthropic Messages API
        if (obj.contains("content")) {
            QJsonArray content = obj["content"].toArray();
            for (const auto &item : content) {
                QJsonObject block = item.toObject();
                if (block["type"].toString() == "text") {
                    addMessage(ChatBubble::AI, block["text"].toString());
                }
            }
        }
        // OpenAI-compatible API
        else if (obj.contains("choices")) {
            QJsonArray choices = obj["choices"].toArray();
            if (!choices.isEmpty()) {
                QString msg = choices[0].toObject()["message"].toObject()["content"].toString();
                addMessage(ChatBubble::AI, msg);
            }
        }
    }

    reply->deleteLater();
}

void AIChatPanel::scrollToBottom()
{
    QTimer::singleShot(10, this, [this]() {
        auto *sb = m_scrollArea->verticalScrollBar();
        sb->setValue(sb->maximum());
    });
}

void AIChatPanel::applyTheme(bool isDark)
{
    m_isDark = isDark;

    if (isDark) {
        setStyleSheet(QString("background: %1;").arg(BG2));
        m_columnHeader->setStyleSheet(QString(
            "background: %1; border-bottom: 1px solid %2;"
        ).arg(BG2, BORDER));
        m_titleLabel->setStyleSheet(QString(
            "font-size: 12px; font-weight: 700; color: %1; letter-spacing: 0.3px;"
        ).arg(ACCENT));
        m_closeButton->setStyleSheet(QString(
            "QPushButton { background: none; color: %1; font-size: 12px; border: none; }"
            "QPushButton:hover { color: %2; }"
        ).arg(FG3, FG));
        m_headerLabel->setStyleSheet(QString(
            "font-size: 10px; font-weight: 700; text-transform: uppercase; "
            "letter-spacing: 1px; color: %1;"
        ).arg(FG2));
        m_modelSelector->setStyleSheet(QString(
            "QComboBox { font-size: 10px; background: %1; color: %2; "
            "border: 1px solid %3; border-radius: 4px; padding: 2px 6px; }"
            "QComboBox::drop-down { border: none; }"
            "QComboBox QAbstractItemView { background: %1; color: %2; border: 1px solid %3; }"
        ).arg(BG3, FG, BORDER));
        m_configButton->setStyleSheet(QString(
            "QPushButton { background: none; color: %1; font-size: 13px; border: none; }"
            "QPushButton:hover { color: %2; }"
        ).arg(FG3, FG));
        m_scrollArea->setStyleSheet(
            "QScrollArea { border: none; background: transparent; }"
            "QScrollBar:vertical { width: 7px; background: transparent; }"
            "QScrollBar::handle:vertical { background: #3c3c54; border-radius: 4px; }"
        );
        m_placeholderLabel->setStyleSheet(QString(
            "color: %1; font-size: 11px; font-style: italic; padding: 20px;"
        ).arg(FG2));
        m_input->setStyleSheet(QString(
            "QLineEdit { padding: 7px 10px; font-size: 11px; background: %1; "
            "color: %2; border: 1px solid %3; border-radius: 4px; }"
        ).arg(BG3, FG, BORDER));
        m_sendButton->setStyleSheet(QString(
            "QPushButton { background: %1; color: #1e1e2e; font-weight: 600; "
            "padding: 7px 12px; font-size: 11px; border-radius: 4px; }"
            "QPushButton:hover { filter: brightness(1.15); }"
            "QPushButton:disabled { background: %2; color: %3; }"
        ).arg(ACCENT, BG3, FG3));
    } else {
        setStyleSheet("background: #fafafa;");
        m_columnHeader->setStyleSheet(
            "background: #fafafa; border-bottom: 1px solid #e0e0e0;");
        m_titleLabel->setStyleSheet(
            "font-size: 12px; font-weight: 700; color: #333333; letter-spacing: 0.3px;");
        m_closeButton->setStyleSheet(
            "QPushButton { background: none; color: #999999; font-size: 12px; border: none; }"
            "QPushButton:hover { color: #333333; }");
        m_headerLabel->setStyleSheet(
            "font-size: 10px; font-weight: 700; text-transform: uppercase; "
            "letter-spacing: 1px; color: #666666;");
        m_modelSelector->setStyleSheet(
            "QComboBox { font-size: 10px; background: #fafafa; color: #333333; "
            "border: 1px solid #e0e0e0; border-radius: 4px; padding: 2px 6px; }"
            "QComboBox::drop-down { background: #fafafa; border-left: 1px solid #e0e0e0; }"
            "QComboBox QAbstractItemView { background: #ffffff; color: #333333;"
            " border: 1px solid #e0e0e0; selection-background-color: #e0e8ff;"
            " selection-color: #1e1e2e; }");
        m_configButton->setStyleSheet(
            "QPushButton { background: none; color: #666666; font-size: 13px; border: none; }"
            "QPushButton:hover { color: #333333; }");
        m_scrollArea->setStyleSheet(
            "QScrollArea { border: none; background: transparent; }"
            "QScrollBar:vertical { width: 7px; background: #f0f0f0; }"
            "QScrollBar::handle:vertical { background: #cccccc; border-radius: 4px; }"
        );
        m_placeholderLabel->setStyleSheet(
            "color: #888888; font-size: 11px; font-style: italic; padding: 20px;");
        m_input->setStyleSheet(
            "QLineEdit { padding: 7px 10px; font-size: 11px; background: #ffffff; "
            "color: #1e1e2e; border: 1px solid #e0e0e0; border-radius: 4px; }");
        m_sendButton->setStyleSheet(
            "QPushButton { background: #2563eb; color: #ffffff; font-weight: 600; "
            "padding: 7px 12px; font-size: 11px; border-radius: 4px; }"
            "QPushButton:hover { background: #1d4ed8; }"
            "QPushButton:disabled { background: #d0d0d0; color: #999999; }");
    }
}
