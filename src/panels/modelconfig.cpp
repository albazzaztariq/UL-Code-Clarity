#include "panels/modelconfig.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

static const char* BG      = "#1e1e2e";
static const char* BG2     = "#2a2a3c";
static const char* BG3     = "#333348";
static const char* FG      = "#cdd6f4";
static const char* FG2     = "#a6adc8";
static const char* FG3     = "#6c7086";
static const char* ACCENT  = "#89b4fa";
static const char* BORDER  = "#45475a";

ModelConfigDialog::ModelConfigDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("AI Model Configuration");
    setModal(true);
    setMinimumWidth(400);

#ifdef Q_OS_WIN
    {
        BOOL useDark = TRUE;
        DwmSetWindowAttribute(reinterpret_cast<HWND>(winId()), 20, &useDark, sizeof(useDark));
    }
#endif

    setStyleSheet(QString(
        "QDialog { background: %1; color: %2; }"
        "QLabel { color: %2; font-size: 11px; }"
        "QLineEdit { background: %3; color: %2; border: 1px solid %4;"
        "  border-radius: 4px; padding: 5px 8px; font-size: 11px; }"
        "QComboBox { background: %3; color: %2; border: 1px solid %4;"
        "  border-radius: 4px; padding: 4px 8px; font-size: 11px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: %3; color: %2; border: none; outline: none; }"
    ).arg(BG2, FG, BG3, BORDER));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Title
    auto *titleLabel = new QLabel("Configure AI Model", this);
    titleLabel->setStyleSheet(QString(
        "font-size: 13px; font-weight: 700; color: %1;"
    ).arg(ACCENT));
    mainLayout->addWidget(titleLabel);

    // Form
    m_form = new QFormLayout;
    m_form->setSpacing(8);
    m_form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_providerCombo = new QComboBox(this);
    m_providerCombo->addItems({"Anthropic", "OpenAI", "Google", "Ollama", "Custom"});
    connect(m_providerCombo, &QComboBox::currentTextChanged,
            this, &ModelConfigDialog::onProviderChanged);
    m_form->addRow("Provider:", m_providerCombo);

    m_endpointEdit = new QLineEdit(this);
    m_endpointEdit->setPlaceholderText("https://api.anthropic.com/v1/messages");
    m_form->addRow("Endpoint URL:", m_endpointEdit);

    m_apiKeyLabel = new QLabel("API Key:", this);
    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText("sk-...");
    m_form->addRow(m_apiKeyLabel, m_apiKeyEdit);

    m_modelNameEdit = new QLineEdit(this);
    m_modelNameEdit->setPlaceholderText("claude-sonnet-4-20250514");
    m_form->addRow("Model Name:", m_modelNameEdit);

    mainLayout->addLayout(m_form);

    m_ollamaNote = new QLabel(
        "Ollama runs locally — no API key needed. "
        "Make sure Ollama is running on your machine.",
        this);
    m_ollamaNote->setWordWrap(true);
    m_ollamaNote->setStyleSheet(QString(
        "color: %1; font-size: 10px; font-style: italic;"
        "background: %2; border: 1px solid %3;"
        "border-radius: 4px; padding: 6px 8px;"
    ).arg(FG2, BG3, BORDER));
    m_ollamaNote->setVisible(false);
    mainLayout->addWidget(m_ollamaNote);

    // Hint
    auto *hint = new QLabel(
        "Endpoint URL is auto-filled but remains editable (e.g. for proxies).\n"
        "API key is stored in local QSettings (not transmitted elsewhere).",
        this);
    hint->setStyleSheet(QString("font-size: 10px; color: %1;").arg(FG3));
    hint->setWordWrap(true);
    mainLayout->addWidget(hint);

    mainLayout->addStretch();

    // Buttons
    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();

    m_cancelButton = new QPushButton("Cancel", this);
    m_cancelButton->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 6px 16px; font-size: 11px; }"
        "QPushButton:hover { background: %4; }"
    ).arg(BG3, FG2, BORDER, BG2));
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(m_cancelButton);

    m_saveButton = new QPushButton("Save", this);
    m_saveButton->setDefault(true);
    m_saveButton->setStyleSheet(QString(
        "QPushButton { background: %1; color: #1e1e2e; border: none;"
        "  border-radius: 4px; padding: 6px 16px; font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { background: #74a8e8; }"
    ).arg(ACCENT));
    connect(m_saveButton, &QPushButton::clicked, this, &ModelConfigDialog::onSaveClicked);
    btnRow->addWidget(m_saveButton);

    mainLayout->addLayout(btnRow);

    // Load saved settings
    loadFromSettings();
}

void ModelConfigDialog::onProviderChanged(const QString &provider)
{
    setDefaultsForProvider(provider);
}

void ModelConfigDialog::setDefaultsForProvider(const QString &provider)
{
    struct ProviderDefaults { const char *endpoint; const char *model; };
    auto defaults = [&]() -> ProviderDefaults {
        if (provider == "Anthropic") return {"https://api.anthropic.com/v1/messages",              "claude-sonnet-4-20250514"};
        if (provider == "OpenAI")    return {"https://api.openai.com/v1/chat/completions",          "gpt-4o"};
        if (provider == "Google")    return {"https://generativelanguage.googleapis.com/v1beta/models", "gemini-2.0-flash"};
        if (provider == "Ollama")    return {"http://localhost:11434/api/generate",                 "llama3"};
        return {"", ""};  // Custom
    }();

    bool isOllama = (provider == "Ollama");
    bool isCustom = (provider == "Custom");

    if (!isCustom) {
        m_endpointEdit->setText(QString(defaults.endpoint));
        m_modelNameEdit->setText(QString(defaults.model));
    } else {
        m_endpointEdit->clear();
        m_modelNameEdit->clear();
    }

    m_endpointEdit->setPlaceholderText(isCustom ? "https://..." : QString(defaults.endpoint));
    m_modelNameEdit->setPlaceholderText(isCustom ? "model-name" : QString(defaults.model));

    // Show/hide API key row for Ollama
    m_apiKeyLabel->setVisible(!isOllama);
    m_apiKeyEdit->setVisible(!isOllama);
    m_ollamaNote->setVisible(isOllama);

    if (!isOllama) {
        m_apiKeyEdit->setPlaceholderText("sk-...");
    }
}

void ModelConfigDialog::loadFromSettings()
{
    QSettings s("CodeClarity", "CodeClarity");
    QString provider = s.value("ai/provider", "Anthropic").toString();
    int idx = m_providerCombo->findText(provider);
    if (idx >= 0) m_providerCombo->setCurrentIndex(idx);
    setDefaultsForProvider(provider);

    QString savedEndpoint = s.value("ai/endpoint").toString();
    QString savedApiKey   = s.value("ai/apiKey").toString();
    QString savedModel    = s.value("ai/modelName").toString();
    if (!savedEndpoint.isEmpty()) m_endpointEdit->setText(savedEndpoint);
    if (!savedApiKey.isEmpty())   m_apiKeyEdit->setText(savedApiKey);
    if (!savedModel.isEmpty())    m_modelNameEdit->setText(savedModel);
}

void ModelConfigDialog::saveToSettings()
{
    QSettings s("CodeClarity", "CodeClarity");
    s.setValue("ai/provider",   m_providerCombo->currentText());
    s.setValue("ai/endpoint",   m_endpointEdit->text());
    s.setValue("ai/apiKey",     m_apiKeyEdit->text());
    s.setValue("ai/modelName",  m_modelNameEdit->text());
}

ModelConfig ModelConfigDialog::currentConfig() const
{
    return {
        m_providerCombo->currentText(),
        m_endpointEdit->text(),
        m_apiKeyEdit->text(),
        m_modelNameEdit->text()
    };
}

void ModelConfigDialog::onSaveClicked()
{
    saveToSettings();
    accept();
}
