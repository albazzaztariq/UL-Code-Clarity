#include "core/setupwizard.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSettings>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QFont>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

// Theme colours (local copy — same palette as the rest of Code Clarity)
namespace {
    const char* BG     = "#1e1e2e";
    const char* BG2    = "#2a2a3c";
    const char* BG3    = "#333348";
    const char* FG     = "#cdd6f4";
    const char* FG2    = "#a6adc8";
    const char* FG3    = "#6c7086";
    const char* ACCENT = "#89b4fa";
    const char* BORDER = "#45475a";
    const char* GREEN  = "#a6e3a1";
    const char* RED    = "#f38ba8";
}

SetupWizard::SetupWizard(QWidget *parent)
    : QDialog(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    setWindowTitle("Code Clarity Setup");
    setMinimumSize(480, 380);
    setMaximumSize(560, 480);

#ifdef Q_OS_WIN
    {
        BOOL useDark = TRUE;
        DwmSetWindowAttribute(reinterpret_cast<HWND>(winId()), 20, &useDark, sizeof(useDark));
    }
#endif

    setStyleSheet(QString(
        "QDialog { background: %1; color: %2; }"
        "QLabel  { color: %2; font-size: 11px; }"
        "QLineEdit { background: %3; color: %2; border: 1px solid %4;"
        "  border-radius: 4px; padding: 5px 8px; font-size: 11px; }"
        "QComboBox { background: %3; color: %2; border: 1px solid %4;"
        "  border-radius: 4px; padding: 4px 8px; font-size: 11px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: %3; color: %2; border: none; outline: none; }"
    ).arg(BG2, FG, BG3, BORDER));

    // Main layout
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Stacked pages
    m_pages = new QStackedWidget(this);
    buildWelcomePage();
    buildProviderPage();
    buildTestPage();
    buildDonePage();
    mainLayout->addWidget(m_pages, 1);

    // Navigation bar
    auto *navBar = new QWidget(this);
    navBar->setStyleSheet(QString(
        "background: %1; border-top: 1px solid %2;"
    ).arg(BG, BORDER));
    auto *navLayout = new QHBoxLayout(navBar);
    navLayout->setContentsMargins(16, 10, 16, 10);
    navLayout->setSpacing(8);

    m_backBtn = new QPushButton("Back", navBar);
    m_backBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 6px 18px; font-size: 11px; }"
        "QPushButton:hover { background: %4; }"
        "QPushButton:disabled { color: %5; border-color: %3; }"
    ).arg(BG3, FG2, BORDER, BG2, FG3));
    connect(m_backBtn, &QPushButton::clicked, this, &SetupWizard::onBackClicked);

    navLayout->addWidget(m_backBtn);
    navLayout->addStretch();

    m_nextBtn = new QPushButton("Next", navBar);
    m_nextBtn->setDefault(true);
    m_nextBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: #1e1e2e; border: none;"
        "  border-radius: 4px; padding: 6px 22px; font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { background: #74a8e8; }"
    ).arg(ACCENT));
    connect(m_nextBtn, &QPushButton::clicked, this, &SetupWizard::onNextClicked);
    navLayout->addWidget(m_nextBtn);

    m_finishBtn = new QPushButton("Finish", navBar);
    m_finishBtn->setVisible(false);
    m_finishBtn->setStyleSheet(m_nextBtn->styleSheet());
    connect(m_finishBtn, &QPushButton::clicked, this, &QDialog::accept);
    navLayout->addWidget(m_finishBtn);

    mainLayout->addWidget(navBar);

    connect(m_nam, &QNetworkAccessManager::finished, this, &SetupWizard::onTestReply);

    goTo(0);
}

// ── Page builders ─────────────────────────────────────────────────────────

void SetupWizard::buildWelcomePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(40, 40, 40, 24);
    layout->setSpacing(16);

    // Logo / title
    auto *logo = new QLabel("Code Clarity", page);
    QFont logoFont = logo->font();
    logoFont.setPointSize(22);
    logoFont.setBold(true);
    logo->setFont(logoFont);
    logo->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: 700;").arg(ACCENT));
    logo->setAlignment(Qt::AlignCenter);
    layout->addWidget(logo);

    auto *tagline = new QLabel("Your intelligent coding companion", page);
    tagline->setAlignment(Qt::AlignCenter);
    tagline->setStyleSheet(QString("color: %1; font-size: 12px; font-style: italic;").arg(FG2));
    layout->addWidget(tagline);

    layout->addSpacing(12);

    auto *desc = new QLabel(
        "Welcome! Before you start, let's connect Code Clarity to an AI model.\n\n"
        "You can use Anthropic (Claude), OpenAI (GPT-4), Ollama (local), or any "
        "OpenAI-compatible endpoint.\n\n"
        "Click Next to configure your AI provider.",
        page);
    desc->setWordWrap(true);
    desc->setAlignment(Qt::AlignCenter);
    desc->setStyleSheet(QString("color: %1; font-size: 11px; line-height: 1.5;").arg(FG2));
    layout->addWidget(desc);

    layout->addStretch();

    m_pages->addWidget(page);
}

void SetupWizard::buildProviderPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 24, 32, 16);
    layout->setSpacing(16);

    auto *title = new QLabel("Configure AI Provider", page);
    title->setStyleSheet(QString(
        "font-size: 14px; font-weight: 700; color: %1;"
    ).arg(ACCENT));
    layout->addWidget(title);

    auto *sub = new QLabel("Enter your AI provider details. These are saved locally.", page);
    sub->setStyleSheet(QString("color: %1; font-size: 10px;").arg(FG3));
    layout->addWidget(sub);

    auto *form = new QFormLayout;
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_providerCombo = new QComboBox(page);
    m_providerCombo->addItems({"Anthropic", "OpenAI", "Google", "Ollama", "Custom"});
    connect(m_providerCombo, &QComboBox::currentTextChanged,
            this, &SetupWizard::onProviderChanged);
    form->addRow("Provider:", m_providerCombo);

    m_endpointEdit = new QLineEdit(page);
    form->addRow("Endpoint URL:", m_endpointEdit);

    m_apiKeyLabel = new QLabel("API Key:", page);
    m_apiKeyEdit = new QLineEdit(page);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText("sk-...");
    form->addRow(m_apiKeyLabel, m_apiKeyEdit);

    m_modelEdit = new QLineEdit(page);
    form->addRow("Model Name:", m_modelEdit);

    layout->addLayout(form);

    m_ollamaNote = new QLabel(
        "Ollama runs locally — no API key needed. "
        "Make sure Ollama is running on your machine.",
        page);
    m_ollamaNote->setWordWrap(true);
    m_ollamaNote->setStyleSheet(QString(
        "color: %1; font-size: 10px; font-style: italic;"
        "background: %2; border: 1px solid %3;"
        "border-radius: 4px; padding: 6px 8px;"
    ).arg(FG2, BG3, BORDER));
    m_ollamaNote->setVisible(false);
    layout->addWidget(m_ollamaNote);

    auto *hint = new QLabel(
        "Endpoint URL is auto-filled but remains editable (e.g. for proxies).\n"
        "API key is stored only in local app settings — never sent elsewhere.",
        page);
    hint->setWordWrap(true);
    hint->setStyleSheet(QString("color: %1; font-size: 10px;").arg(FG3));
    layout->addWidget(hint);

    layout->addStretch();

    // Trigger placeholder update for default provider
    onProviderChanged("Anthropic");

    // Load from QSettings if previously set
    QSettings s("CodeClarity", "CodeClarity");
    QString saved = s.value("ai/provider", "").toString();
    if (!saved.isEmpty()) {
        int idx = m_providerCombo->findText(saved);
        if (idx >= 0) m_providerCombo->setCurrentIndex(idx);
        QString savedEndpoint = s.value("ai/endpoint").toString();
        QString savedApiKey   = s.value("ai/apiKey").toString();
        QString savedModel    = s.value("ai/modelName").toString();
        if (!savedEndpoint.isEmpty()) m_endpointEdit->setText(savedEndpoint);
        if (!savedApiKey.isEmpty())   m_apiKeyEdit->setText(savedApiKey);
        if (!savedModel.isEmpty())    m_modelEdit->setText(savedModel);
    }

    m_pages->addWidget(page);
}

void SetupWizard::buildTestPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 24, 32, 16);
    layout->setSpacing(14);

    auto *title = new QLabel("Test Connection", page);
    title->setStyleSheet(QString(
        "font-size: 14px; font-weight: 700; color: %1;"
    ).arg(ACCENT));
    layout->addWidget(title);

    auto *sub = new QLabel(
        "A working AI connection is required. Test your configuration below — "
        "you must pass this step to continue.",
        page);
    sub->setWordWrap(true);
    sub->setStyleSheet(QString("color: %1; font-size: 11px;").arg(FG2));
    layout->addWidget(sub);

    // Button row: Test + Try Again (shown after failure)
    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    m_testBtn = new QPushButton("Test Connection", page);
    m_testBtn->setFixedHeight(34);
    m_testBtn->setCursor(Qt::PointingHandCursor);
    m_testBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: #1e1e2e; border: none;"
        "  border-radius: 4px; padding: 6px 20px; font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { background: #74a8e8; }"
        "QPushButton:disabled { background: %2; color: %3; }"
    ).arg(ACCENT, BG3, FG3));
    connect(m_testBtn, &QPushButton::clicked, this, &SetupWizard::onTestClicked);
    btnRow->addWidget(m_testBtn);

    m_tryAgainBtn = new QPushButton("Try Again", page);
    m_tryAgainBtn->setFixedHeight(34);
    m_tryAgainBtn->setCursor(Qt::PointingHandCursor);
    m_tryAgainBtn->setVisible(false);
    m_tryAgainBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 6px 16px; font-size: 11px; }"
        "QPushButton:hover { background: %4; }"
    ).arg(BG3, FG2, BORDER, BG2));
    connect(m_tryAgainBtn, &QPushButton::clicked, this, &SetupWizard::onTestClicked);
    btnRow->addWidget(m_tryAgainBtn);

    btnRow->addStretch();
    layout->addLayout(btnRow);

    m_testStatus = new QLabel("", page);
    m_testStatus->setWordWrap(true);
    m_testStatus->setStyleSheet(QString("font-size: 11px; color: %1;").arg(FG2));
    layout->addWidget(m_testStatus);

    m_testResponse = new QLabel("", page);
    m_testResponse->setWordWrap(true);
    m_testResponse->setStyleSheet(QString(
        "background: %1; color: %2; border: 1px solid %3;"
        "border-radius: 4px; padding: 8px 10px; font-size: 11px;"
    ).arg(BG3, FG, BORDER));
    m_testResponse->setVisible(false);
    layout->addWidget(m_testResponse);

    layout->addStretch();

    m_pages->addWidget(page);
}

void SetupWizard::buildDonePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(40, 40, 40, 24);
    layout->setSpacing(16);

    auto *icon = new QLabel(QString::fromUtf8("\xe2\x9c\x94"), page); // ✔
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(QString("color: %1; font-size: 48px;").arg(GREEN));
    layout->addWidget(icon);

    auto *title = new QLabel("Setup Complete!", page);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QString(
        "font-size: 16px; font-weight: 700; color: %1;"
    ).arg(ACCENT));
    layout->addWidget(title);

    auto *desc = new QLabel(
        "Your AI model is configured and ready to use.\n\n"
        "Open any file in the editor and start chatting in the AI Chat panel.\n\n"
        "You can change settings anytime via the gear icon in the chat panel.",
        page);
    desc->setWordWrap(true);
    desc->setAlignment(Qt::AlignCenter);
    desc->setStyleSheet(QString("color: %1; font-size: 11px;").arg(FG2));
    layout->addWidget(desc);

    layout->addStretch();

    m_pages->addWidget(page);
}

// ── Navigation ────────────────────────────────────────────────────────────

void SetupWizard::goTo(int page)
{
    m_currentPage = page;
    m_pages->setCurrentIndex(page);
    updateNavButtons();
}

void SetupWizard::updateNavButtons()
{
    bool isLast = (m_currentPage == PAGE_COUNT - 1);
    bool isTest = (m_currentPage == 2);  // test page: Next visible but disabled until pass

    m_backBtn->setEnabled(m_currentPage > 0 && !isLast);
    m_nextBtn->setVisible(!isLast);
    m_nextBtn->setEnabled(!isTest || m_testPassed);
    m_finishBtn->setVisible(isLast);

    m_nextBtn->setText(m_currentPage == 0 ? "Get Started" : "Next");
}

void SetupWizard::onNextClicked()
{
    if (m_currentPage == 1) {
        saveToSettings();
    }
    if (m_currentPage < PAGE_COUNT - 1) {
        goTo(m_currentPage + 1);
    }
}

void SetupWizard::onBackClicked()
{
    if (m_currentPage > 0) {
        goTo(m_currentPage - 1);
    }
}

// ── Provider change ───────────────────────────────────────────────────────

void SetupWizard::onProviderChanged(const QString &provider)
{
    bool isOllama  = (provider == "Ollama");
    bool isCustom  = (provider == "Custom");

    // Auto-fill endpoint and model (leave blank for Custom so user must fill in)
    if (!isCustom) {
        m_endpointEdit->setText(defaultEndpoint(provider));
        m_modelEdit->setText(defaultModel(provider));
    } else {
        m_endpointEdit->clear();
        m_modelEdit->clear();
    }

    m_endpointEdit->setPlaceholderText(isCustom ? "https://..." : defaultEndpoint(provider));
    m_modelEdit->setPlaceholderText(isCustom ? "model-name" : defaultModel(provider));

    // Show/hide API key row for Ollama
    m_apiKeyLabel->setVisible(!isOllama);
    m_apiKeyEdit->setVisible(!isOllama);
    m_ollamaNote->setVisible(isOllama);

    if (!isOllama) {
        m_apiKeyEdit->setPlaceholderText("sk-...");
    }
}

QString SetupWizard::defaultEndpoint(const QString &provider) const
{
    if (provider == "Anthropic") return "https://api.anthropic.com/v1/messages";
    if (provider == "OpenAI")    return "https://api.openai.com/v1/chat/completions";
    if (provider == "Google")    return "https://generativelanguage.googleapis.com/v1beta/models";
    if (provider == "Ollama")    return "http://localhost:11434/api/generate";
    return "https://api.openai.com/v1/chat/completions";
}

QString SetupWizard::defaultModel(const QString &provider) const
{
    if (provider == "Anthropic") return "claude-sonnet-4-20250514";
    if (provider == "OpenAI")    return "gpt-4o";
    if (provider == "Google")    return "gemini-2.0-flash";
    if (provider == "Ollama")    return "llama3";
    return "gpt-4o";
}

// ── Settings ──────────────────────────────────────────────────────────────

void SetupWizard::saveToSettings()
{
    QSettings s("CodeClarity", "CodeClarity");
    s.setValue("ai/provider",   m_providerCombo->currentText());
    s.setValue("ai/endpoint",   m_endpointEdit->text());
    s.setValue("ai/apiKey",     m_apiKeyEdit->text());
    s.setValue("ai/modelName",  m_modelEdit->text());
}

// ── Test connection ───────────────────────────────────────────────────────

void SetupWizard::onTestClicked()
{
    // Save settings first so we test exactly what's configured
    saveToSettings();

    m_testBtn->setEnabled(false);
    m_testStatus->setStyleSheet(QString("font-size: 11px; color: %1;").arg(FG2));
    m_testStatus->setText("Sending test message...");
    m_testResponse->setVisible(false);

    QSettings s("CodeClarity", "CodeClarity");
    QString provider  = s.value("ai/provider",  "Anthropic").toString();
    QString endpoint  = s.value("ai/endpoint",  "").toString();
    QString apiKey    = s.value("ai/apiKey",    "").toString();
    QString modelName = s.value("ai/modelName", "").toString();

    if (endpoint.isEmpty())  endpoint  = defaultEndpoint(provider);
    if (modelName.isEmpty()) modelName = defaultModel(provider);

    // Build a minimal "Say hello" request
    QByteArray body;
    if (provider == "Anthropic") {
        QJsonObject obj;
        obj["model"]      = modelName;
        obj["max_tokens"] = 64;
        obj["stream"]     = false;
        QJsonArray msgs;
        QJsonObject msg;
        msg["role"]    = "user";
        msg["content"] = "Say hello in exactly 5 words.";
        msgs.append(msg);
        obj["messages"] = msgs;
        body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    } else if (provider == "Ollama") {
        QJsonObject obj;
        obj["model"]  = modelName;
        obj["prompt"] = "Say hello in exactly 5 words.";
        obj["stream"] = false;
        body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    } else {
        // OpenAI-compatible
        QJsonObject obj;
        obj["model"]  = modelName;
        obj["stream"] = false;
        QJsonArray msgs;
        QJsonObject msg;
        msg["role"]    = "user";
        msg["content"] = "Say hello in exactly 5 words.";
        msgs.append(msg);
        obj["messages"] = msgs;
        body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

    QUrl reqUrl(endpoint);
    QNetworkRequest req(reqUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (provider == "Anthropic") {
        req.setRawHeader("x-api-key", apiKey.toUtf8());
        req.setRawHeader("anthropic-version", "2023-06-01");
    } else if (provider != "Ollama") {
        req.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());
    }

    m_nam->post(req, body);
}

void SetupWizard::onTestReply(QNetworkReply *reply)
{
    m_testBtn->setEnabled(true);
    reply->deleteLater();

    QByteArray data = reply->readAll();

    if (reply->error() != QNetworkReply::NoError) {
        m_testStatus->setStyleSheet(QString("font-size: 11px; color: %1;").arg(RED));
        m_testStatus->setText(QString("Connection failed: %1\n"
                                      "Go back, fix your configuration, and try again.")
                              .arg(reply->errorString()));
        if (!data.isEmpty()) {
            m_testResponse->setText(QString::fromUtf8(data.left(300)));
            m_testResponse->setVisible(true);
        }
        return;
    }

    // Parse response
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QString responseText;

    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        // Anthropic
        if (obj.contains("content")) {
            QJsonArray content = obj["content"].toArray();
            for (const auto &item : content) {
                QJsonObject block = item.toObject();
                if (block["type"].toString() == "text")
                    responseText += block["text"].toString();
            }
        }
        // OpenAI-compatible
        else if (obj.contains("choices")) {
            QJsonArray choices = obj["choices"].toArray();
            if (!choices.isEmpty()) {
                responseText = choices[0].toObject()["message"].toObject()["content"].toString();
            }
        }
        // Ollama non-streaming
        else if (obj.contains("response")) {
            responseText = obj["response"].toString();
        }
    }

    if (responseText.isEmpty()) {
        m_testStatus->setStyleSheet(QString("font-size: 11px; color: %1;").arg(RED));
        m_testStatus->setText("Connected but got an unexpected response format. "
                              "Fix your configuration and try again.");
        m_testResponse->setText(QString::fromUtf8(data.left(400)));
        m_testResponse->setVisible(true);
    } else {
        m_testStatus->setStyleSheet(QString("font-size: 11px; color: %1;").arg(GREEN));
        m_testStatus->setText("Connection successful!");
        m_testResponse->setText("Model said: " + responseText.trimmed());
        m_testResponse->setVisible(true);
        // Advance to the done page automatically — test passed.
        goTo(3);
    }
}
