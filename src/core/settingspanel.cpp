#include "core/settingspanel.h"
#include "core/theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSettings>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QFontDatabase>
#include <QDialogButtonBox>
#include <QFrame>
#include <QScrollArea>
#include <QApplication>
#include <QIntValidator>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

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

    QString dialogStyle() {
        return QString(
            "QDialog { background: %1; color: %2; }"
            "QTabWidget::pane { border: 1px solid %3; background: %1; }"
            "QTabBar::tab { background: %4; color: %5; padding: 6px 16px; "
            "  border: 1px solid %3; border-bottom: none; border-radius: 4px 4px 0 0; }"
            "QTabBar::tab:selected { background: %1; color: %2; }"
            "QLabel { color: %2; font-size: 11px; }"
            "QLineEdit { background: %4; color: %2; border: 1px solid %3;"
            "  border-radius: 4px; padding: 5px 8px; font-size: 11px; }"
            "QTextEdit { background: %4; color: %2; border: 1px solid %3;"
            "  border-radius: 4px; padding: 4px 8px; font-size: 11px; }"
            "QComboBox { background: %4; color: %2; border: 1px solid %3;"
            "  border-radius: 4px; padding: 4px 8px; font-size: 11px; }"
            "QComboBox::drop-down { border: none; }"
            "QComboBox QAbstractItemView { background: %4; color: %2; border: none; outline: none; }"
            "QCheckBox { color: %2; font-size: 11px; }"
            "QCheckBox::indicator { width: 14px; height: 14px; }"
            "QListWidget { background: %4; color: %2; border: 1px solid %3;"
            "  border-radius: 4px; font-size: 11px; }"
            "QListWidget::item:selected { background: %6; color: #1e1e2e; }"
        ).arg(BG2, FG, BORDER, BG3, FG2, ACCENT);
    }

    QString buttonStyle(bool primary = false) {
        if (primary) {
            return QString(
                "QPushButton { background: %1; color: #1e1e2e; border: none;"
                "  border-radius: 4px; padding: 6px 18px; font-size: 11px; font-weight: 600; }"
                "QPushButton:hover { background: #74a8e8; }"
                "QPushButton:disabled { background: %2; color: %3; }"
            ).arg(ACCENT, BG3, FG3);
        }
        return QString(
            "QPushButton { background: %1; color: %2; border: 1px solid %3;"
            "  border-radius: 4px; padding: 6px 14px; font-size: 11px; }"
            "QPushButton:hover { background: %4; }"
            "QPushButton:disabled { color: %5; }"
        ).arg(BG3, FG2, BORDER, BG2, FG3);
    }
}

// ============================================================================
// Static model list persistence
// ============================================================================

QList<SavedModel> SettingsPanel::loadModels()
{
    QSettings s("CodeClarity", "CodeClarity");
    int count = s.beginReadArray("ai/models");
    QList<SavedModel> models;
    for (int i = 0; i < count; ++i) {
        s.setArrayIndex(i);
        SavedModel m;
        m.provider  = s.value("provider",  "Anthropic").toString();
        m.endpoint  = s.value("endpoint",  "").toString();
        m.apiKey    = s.value("apiKey",    "").toString();
        m.modelName = s.value("modelName", "").toString();
        m.validated = s.value("validated", false).toBool();
        models.append(m);
    }
    s.endArray();

    // If no models saved yet, migrate the legacy ai/* single-model settings
    if (models.isEmpty()) {
        QString provider = s.value("ai/provider", "").toString();
        if (!provider.isEmpty()) {
            SavedModel m;
            m.provider  = provider;
            m.endpoint  = s.value("ai/endpoint",  "").toString();
            m.apiKey    = s.value("ai/apiKey",    "").toString();
            m.modelName = s.value("ai/modelName", "").toString();
            m.validated = false;
            models.append(m);
            saveModels(models);
        }
    }

    return models;
}

void SettingsPanel::saveModels(const QList<SavedModel> &models)
{
    QSettings s("CodeClarity", "CodeClarity");
    s.beginWriteArray("ai/models", models.size());
    for (int i = 0; i < models.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue("provider",  models[i].provider);
        s.setValue("endpoint",  models[i].endpoint);
        s.setValue("apiKey",    models[i].apiKey);
        s.setValue("modelName", models[i].modelName);
        s.setValue("validated", models[i].validated);
    }
    s.endArray();

    // Keep legacy keys pointing at the first model for backwards compat
    if (!models.isEmpty()) {
        s.setValue("ai/provider",  models[0].provider);
        s.setValue("ai/endpoint",  models[0].endpoint);
        s.setValue("ai/apiKey",    models[0].apiKey);
        s.setValue("ai/modelName", models[0].modelName);
    }
}

void SettingsPanel::addOrUpdateModel(const SavedModel &m)
{
    QList<SavedModel> models = loadModels();
    // Check for duplicate by modelName+provider
    for (auto &existing : models) {
        if (existing.provider == m.provider && existing.modelName == m.modelName) {
            existing = m;
            saveModels(models);
            return;
        }
    }
    models.append(m);
    saveModels(models);
}

// ============================================================================
// SettingsPanel
// ============================================================================

SettingsPanel::SettingsPanel(QWidget *parent)
    : QDialog(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    setWindowTitle("Settings");
    setModal(true);
    setMinimumSize(560, 440);
    setStyleSheet(dialogStyle());

#ifdef Q_OS_WIN
    {
        BOOL useDark = TRUE;
        DwmSetWindowAttribute(reinterpret_cast<HWND>(winId()), 20, &useDark, sizeof(useDark));
    }
#endif

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_tabs = new QTabWidget(this);
    buildGeneralTab(m_tabs);
    buildModelsTab(m_tabs);
    buildAIBehaviorTab(m_tabs);
    buildPermissionsTab(m_tabs);
    mainLayout->addWidget(m_tabs, 1);

    // Bottom bar
    auto *bottomBar = new QWidget(this);
    bottomBar->setStyleSheet(QString("background: %1; border-top: 1px solid %2;").arg(BG, BORDER));
    auto *btnRow = new QHBoxLayout(bottomBar);
    btnRow->setContentsMargins(16, 10, 16, 10);
    btnRow->addStretch();

    auto *cancelBtn = new QPushButton("Cancel", bottomBar);
    cancelBtn->setStyleSheet(buttonStyle(false));
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(cancelBtn);

    auto *saveBtn = new QPushButton("Save", bottomBar);
    saveBtn->setDefault(true);
    saveBtn->setStyleSheet(buttonStyle(true));
    connect(saveBtn, &QPushButton::clicked, this, &SettingsPanel::onSaveClicked);
    btnRow->addWidget(saveBtn);

    mainLayout->addWidget(bottomBar);

    connect(m_nam, &QNetworkAccessManager::finished, this, &SettingsPanel::onTestReply);

    loadSettings();
    refreshModelList();
}

void SettingsPanel::openToTab(int index)
{
    if (m_tabs && index >= 0 && index < m_tabs->count())
        m_tabs->setCurrentIndex(index);
}

// ── Tab builders ──────────────────────────────────────────────────────────

void SettingsPanel::buildGeneralTab(QTabWidget *tabs)
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);

    auto *form = new QFormLayout;
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_themeCombo = new QComboBox(page);
    m_themeCombo->addItems({"Dark", "Light"});
    form->addRow("Theme:", m_themeCombo);

    m_fontFamilyCombo = new QComboBox(page);
    QFontDatabase db;
    for (const QString &family : db.families(QFontDatabase::Any)) {
        m_fontFamilyCombo->addItem(family);
    }
    form->addRow("Editor Font:", m_fontFamilyCombo);

    // Custom font size widget: [▼] [value] [▲]
    auto *fontSizeWidget = new QWidget(page);
    auto *fontSizeLayout = new QHBoxLayout(fontSizeWidget);
    fontSizeLayout->setContentsMargins(0, 0, 0, 0);
    fontSizeLayout->setSpacing(2);

    auto *fontSizeDown = new QPushButton("\xe2\x96\xbc", fontSizeWidget); // ▼
    fontSizeDown->setFixedSize(24, 24);
    fontSizeDown->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 3px; font-size: 11px; padding: 0; }"
        "QPushButton:hover { background: %4; }"
    ).arg(BG3, FG, BORDER, BG2));

    m_fontSizeEdit = new QLineEdit(fontSizeWidget);
    m_fontSizeEdit->setText("11");
    m_fontSizeEdit->setFixedWidth(40);
    m_fontSizeEdit->setAlignment(Qt::AlignCenter);
    m_fontSizeEdit->setValidator(new QIntValidator(8, 24, m_fontSizeEdit));
    m_fontSizeEdit->setStyleSheet(QString(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 3px; padding: 2px 4px; font-size: 11px; }"
        "QLineEdit:focus { border-color: %4; }"
    ).arg(BG3, FG, BORDER, ACCENT));

    auto *fontSizeUp = new QPushButton("\xe2\x96\xb2", fontSizeWidget); // ▲
    fontSizeUp->setFixedSize(24, 24);
    fontSizeUp->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 3px; font-size: 11px; padding: 0; }"
        "QPushButton:hover { background: %4; }"
    ).arg(BG3, FG, BORDER, BG2));

    connect(fontSizeDown, &QPushButton::clicked, this, [this]() {
        int v = m_fontSizeEdit->text().toInt();
        if (v > 8) m_fontSizeEdit->setText(QString::number(v - 1));
    });
    connect(fontSizeUp, &QPushButton::clicked, this, [this]() {
        int v = m_fontSizeEdit->text().toInt();
        if (v < 24) m_fontSizeEdit->setText(QString::number(v + 1));
    });

    fontSizeLayout->addWidget(fontSizeDown);
    fontSizeLayout->addWidget(m_fontSizeEdit);
    fontSizeLayout->addWidget(fontSizeUp);
    fontSizeLayout->addStretch();
    form->addRow("Font Size:", fontSizeWidget);

    layout->addLayout(form);
    layout->addStretch();


    tabs->addTab(page, "General");
}

void SettingsPanel::buildModelsTab(QTabWidget *tabs)
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto *hdr = new QLabel(
        "All models appear in the AI Chat dropdown. "
        "Validated models show a checkmark.", page);
    hdr->setWordWrap(true);
    hdr->setStyleSheet(QString("color: %1; font-size: 10px;").arg(FG3));
    layout->addWidget(hdr);

    m_modelList = new QListWidget(page);
    layout->addWidget(m_modelList, 1);

    // Button row
    auto *btnRow = new QHBoxLayout;
    m_addBtn = new QPushButton("Add", page);
    m_addBtn->setStyleSheet(buttonStyle());
    connect(m_addBtn, &QPushButton::clicked, this, &SettingsPanel::onAddModel);
    btnRow->addWidget(m_addBtn);

    m_editBtn = new QPushButton("Edit", page);
    m_editBtn->setStyleSheet(buttonStyle());
    m_editBtn->setEnabled(false);
    connect(m_editBtn, &QPushButton::clicked, this, &SettingsPanel::onEditModel);
    btnRow->addWidget(m_editBtn);

    m_removeBtn = new QPushButton("Remove", page);
    m_removeBtn->setStyleSheet(buttonStyle());
    m_removeBtn->setEnabled(false);
    connect(m_removeBtn, &QPushButton::clicked, this, &SettingsPanel::onRemoveModel);
    btnRow->addWidget(m_removeBtn);

    btnRow->addStretch();

    m_testBtn = new QPushButton("Test Connection", page);
    m_testBtn->setStyleSheet(buttonStyle(true));
    m_testBtn->setEnabled(false);
    connect(m_testBtn, &QPushButton::clicked, this, &SettingsPanel::onTestModel);
    btnRow->addWidget(m_testBtn);

    layout->addLayout(btnRow);

    m_testStatus = new QLabel("", page);
    m_testStatus->setStyleSheet(QString("font-size: 10px; color: %1;").arg(FG2));
    layout->addWidget(m_testStatus);

    // Enable edit/remove/test when a row is selected
    connect(m_modelList, &QListWidget::itemSelectionChanged, this, [this]() {
        bool hasSelection = !m_modelList->selectedItems().isEmpty();
        m_editBtn->setEnabled(hasSelection);
        m_removeBtn->setEnabled(hasSelection);
        m_testBtn->setEnabled(hasSelection);
        m_testStatus->clear();
    });

    tabs->addTab(page, "Models");
}

void SettingsPanel::buildAIBehaviorTab(QTabWidget *tabs)
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);

    m_webSearchChk = new QCheckBox("Enable web search (costs extra tokens)", page);
    layout->addWidget(m_webSearchChk);

    auto *promptLabel = new QLabel("Custom system prompt (appended to the default):", page);
    layout->addWidget(promptLabel);

    m_systemPromptEdit = new QTextEdit(page);
    m_systemPromptEdit->setPlaceholderText(
        "Additional instructions for the AI assistant, e.g. "
        "\"Always explain changes in plain English.\"");
    m_systemPromptEdit->setMaximumHeight(120);
    layout->addWidget(m_systemPromptEdit);

    auto *note = new QLabel(
        "The default system prompt tells the AI the file language and content. "
        "Your custom prompt is appended after it.", page);
    note->setWordWrap(true);
    note->setStyleSheet(QString("color: %1; font-size: 10px;").arg(FG3));
    layout->addWidget(note);

    layout->addStretch();

    tabs->addTab(page, "AI Behavior");
}

void SettingsPanel::buildPermissionsTab(QTabWidget *tabs)
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);

    auto *title = new QLabel("File Access Permissions", page);
    title->setStyleSheet(QString("font-size: 13px; font-weight: 700; color: %1;").arg(ACCENT));
    layout->addWidget(title);

    auto *desc = new QLabel(
        "The AI can only access files inside the folders you have added to your workspace. "
        "Access to system directories (/bin, /system32, AppData, etc.) is always blocked.\n\n"
        "By default, Code Clarity asks before each file read or write. "
        "You can disable these prompts below.", page);
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("color: %1; font-size: 11px;").arg(FG2));
    layout->addWidget(desc);

    auto *sep = new QFrame(page);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("color: %1;").arg(BORDER));
    layout->addWidget(sep);

    m_disablePromptsChk = new QCheckBox(
        "Disable all permission prompts for workspace files", page);
    layout->addWidget(m_disablePromptsChk);

    auto *warn = new QLabel(
        "When disabled, the AI can read and write any file in your workspace "
        "without asking. Only enable this if you trust the AI with your codebase.", page);
    warn->setWordWrap(true);
    warn->setStyleSheet(QString("color: %1; font-size: 10px; font-style: italic;").arg(FG3));
    layout->addWidget(warn);

    layout->addStretch();

    tabs->addTab(page, "Permissions");
}

// ── Settings load/save ────────────────────────────────────────────────────

void SettingsPanel::loadSettings()
{
    QSettings s("CodeClarity", "CodeClarity");

    // General
    QString theme = s.value("ui/theme", "Dark").toString();
    m_themeCombo->setCurrentText(theme);

    int fontSize = s.value("ui/fontSize", 11).toInt();
    m_fontSizeEdit->setText(QString::number(fontSize));

    QString fontFamily = s.value("ui/fontFamily", "Consolas").toString();
    int idx = m_fontFamilyCombo->findText(fontFamily);
    if (idx >= 0) m_fontFamilyCombo->setCurrentIndex(idx);

    // AI Behavior
    m_webSearchChk->setChecked(s.value("ai/webSearch", false).toBool());
    m_systemPromptEdit->setPlainText(s.value("ai/customSystemPrompt", "").toString());

    // Permissions
    m_disablePromptsChk->setChecked(s.value("permissions/disablePrompts", false).toBool());
}

void SettingsPanel::applySettings()
{
    QSettings s("CodeClarity", "CodeClarity");

    s.setValue("ui/theme",      m_themeCombo->currentText());
    s.setValue("ui/fontSize",   m_fontSizeEdit->text().toInt());
    s.setValue("ui/fontFamily", m_fontFamilyCombo->currentText());

    s.setValue("ai/webSearch",          m_webSearchChk->isChecked());
    s.setValue("ai/customSystemPrompt", m_systemPromptEdit->toPlainText());

    s.setValue("permissions/disablePrompts", m_disablePromptsChk->isChecked());

    // Save model list
    saveModels(m_models);
}

void SettingsPanel::onSaveClicked()
{
    // Apply theme instantly before saving
    QString theme = m_themeCombo->currentText();
    if (theme == "Dark") {
        qApp->setStyleSheet(Theme::appStyleSheet());
#ifdef Q_OS_WIN
        if (parentWidget()) {
            BOOL useDark = TRUE;
            DwmSetWindowAttribute(reinterpret_cast<HWND>(parentWidget()->winId()), 20, &useDark, sizeof(useDark));
        }
#endif
    } else {
        qApp->setStyleSheet(Theme::lightStyleSheet());
#ifdef Q_OS_WIN
        if (parentWidget()) {
            BOOL useDark = FALSE;
            DwmSetWindowAttribute(reinterpret_cast<HWND>(parentWidget()->winId()), 20, &useDark, sizeof(useDark));
        }
#endif
    }

    applySettings();
    emit modelsChanged();
    accept();
}

// ── Model list management ─────────────────────────────────────────────────

void SettingsPanel::refreshModelList()
{
    m_models = loadModels();
    m_modelList->clear();

    for (const SavedModel &m : m_models) {
        QString display;
        if (!m.modelName.isEmpty())
            display = m.modelName;
        else
            display = m.provider;

        QString validated = m.validated ? " \342\234\224" : "";  // ✔
        m_modelList->addItem(QString("[%1] %2%3").arg(m.provider, display, validated));
    }
}

void SettingsPanel::onAddModel()
{
    ModelEditDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_models.append(dlg.model());
        refreshModelList();
    }
}

void SettingsPanel::onEditModel()
{
    int row = m_modelList->currentRow();
    if (row < 0 || row >= m_models.size()) return;

    ModelEditDialog dlg(this, m_models[row]);
    if (dlg.exec() == QDialog::Accepted) {
        m_models[row] = dlg.model();
        refreshModelList();
        m_modelList->setCurrentRow(row);
    }
}

void SettingsPanel::onRemoveModel()
{
    int row = m_modelList->currentRow();
    if (row < 0 || row >= m_models.size()) return;
    m_models.removeAt(row);
    refreshModelList();
}

// ── Test connection ───────────────────────────────────────────────────────

void SettingsPanel::onTestModel()
{
    int row = m_modelList->currentRow();
    if (row < 0 || row >= m_models.size()) return;

    const SavedModel &m = m_models[row];

    m_testBtn->setEnabled(false);
    m_testStatus->setStyleSheet(QString("font-size: 10px; color: %1;").arg(FG2));
    m_testStatus->setText("Testing connection...");

    QString endpoint = m.endpoint;
    QString modelName = m.modelName;

    // Fill defaults
    if (endpoint.isEmpty()) {
        if (m.provider == "Anthropic")       endpoint = "https://api.anthropic.com/v1/messages";
        else if (m.provider == "OpenAI")     endpoint = "https://api.openai.com/v1/chat/completions";
        else if (m.provider == "Google")     endpoint = "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions";
        else if (m.provider == "Ollama")     endpoint = "http://localhost:11434/api/generate";
        else                                 endpoint = "https://api.openai.com/v1/chat/completions";
    }
    if (modelName.isEmpty()) {
        if (m.provider == "Anthropic")  modelName = "claude-sonnet-4-6";
        else if (m.provider == "OpenAI") modelName = "gpt-4o";
        else if (m.provider == "Ollama") modelName = "llama3";
        else                             modelName = "gpt-4o";
    }

    // Build body
    QByteArray body;
    if (m.provider == "Anthropic") {
        QJsonObject obj;
        obj["model"]      = modelName;
        obj["max_tokens"] = 32;
        obj["stream"]     = false;
        QJsonArray msgs;
        QJsonObject msg; msg["role"] = "user"; msg["content"] = "Say hello.";
        msgs.append(msg);
        obj["messages"] = msgs;
        body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    } else if (m.provider == "Ollama") {
        QJsonObject obj;
        obj["model"]  = modelName;
        obj["prompt"] = "Say hello.";
        obj["stream"] = false;
        body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    } else {
        QJsonObject obj;
        obj["model"]  = modelName;
        obj["stream"] = false;
        QJsonArray msgs;
        QJsonObject msg; msg["role"] = "user"; msg["content"] = "Say hello.";
        msgs.append(msg);
        obj["messages"] = msgs;
        body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

    QUrl reqUrl(endpoint);
    QNetworkRequest req(reqUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (m.provider == "Anthropic") {
        req.setRawHeader("x-api-key", m.apiKey.toUtf8());
        req.setRawHeader("anthropic-version", "2023-06-01");
    } else if (m.provider != "Ollama") {
        req.setRawHeader("Authorization", ("Bearer " + m.apiKey).toUtf8());
    }

    // Store which row we're testing so onTestReply can mark it validated
    req.setAttribute(QNetworkRequest::User, row);
    m_nam->post(req, body);
}

void SettingsPanel::onTestReply(QNetworkReply *reply)
{
    m_testBtn->setEnabled(true);
    reply->deleteLater();

    int row = reply->request().attribute(QNetworkRequest::User).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        m_testStatus->setStyleSheet(QString("font-size: 10px; color: %1;").arg(RED));
        m_testStatus->setText("Failed: " + reply->errorString());
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QString text;
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("content")) {
            text = obj["content"].toArray().first().toObject()["text"].toString();
        } else if (obj.contains("choices")) {
            text = obj["choices"].toArray().first().toObject()
                       ["message"].toObject()["content"].toString();
        } else if (obj.contains("response")) {
            text = obj["response"].toString();
        }
    }

    if (!text.isEmpty()) {
        m_testStatus->setStyleSheet(QString("font-size: 10px; color: %1;").arg(GREEN));
        m_testStatus->setText(QString("Connected! Response: \"%1\"").arg(text.left(80)));
        // Mark model as validated
        if (row >= 0 && row < m_models.size()) {
            m_models[row].validated = true;
            refreshModelList();
            m_modelList->setCurrentRow(row);
        }
    } else {
        m_testStatus->setStyleSheet(QString("font-size: 10px; color: %1;").arg(RED));
        m_testStatus->setText("Connected but unexpected response format.");
    }
}

// ============================================================================
// ModelEditDialog
// ============================================================================

ModelEditDialog::ModelEditDialog(QWidget *parent, const SavedModel &existing)
    : QDialog(parent)
{
    setWindowTitle(existing.provider.isEmpty() ? "Add Model" : "Edit Model");
    setModal(true);
    setMinimumWidth(420);

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
    ).arg("#2a2a3c", "#cdd6f4", "#333348", "#45475a"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);

    auto *form = new QFormLayout;
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_providerCombo = new QComboBox(this);
    m_providerCombo->addItems({"Anthropic", "OpenAI", "Google", "Ollama", "Custom"});
    form->addRow("Provider:", m_providerCombo);

    m_endpointEdit = new QLineEdit(this);
    form->addRow("Endpoint URL:", m_endpointEdit);

    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    form->addRow("API Key:", m_apiKeyEdit);

    m_modelEdit = new QLineEdit(this);
    form->addRow("Model Name:", m_modelEdit);

    layout->addLayout(form);

    // Fill in existing values
    if (!existing.provider.isEmpty()) {
        int idx = m_providerCombo->findText(existing.provider);
        if (idx >= 0) m_providerCombo->setCurrentIndex(idx);
        m_endpointEdit->setText(existing.endpoint);
        m_apiKeyEdit->setText(existing.apiKey);
        m_modelEdit->setText(existing.modelName);
    }

    connect(m_providerCombo, &QComboBox::currentTextChanged,
            this, &ModelEditDialog::onProviderChanged);
    onProviderChanged(m_providerCombo->currentText());

    // Buttons
    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();

    auto *cancelBtn = new QPushButton("Cancel", this);
    cancelBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 6px 14px; font-size: 11px; }"
    ).arg("#333348", "#a6adc8", "#45475a"));
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(cancelBtn);

    auto *okBtn = new QPushButton("OK", this);
    okBtn->setDefault(true);
    okBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: #1e1e2e; border: none;"
        "  border-radius: 4px; padding: 6px 18px; font-size: 11px; font-weight: 600; }"
    ).arg("#89b4fa"));
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(okBtn);

    layout->addLayout(btnRow);
}

SavedModel ModelEditDialog::model() const
{
    SavedModel m;
    m.provider  = m_providerCombo->currentText();
    m.endpoint  = m_endpointEdit->text();
    m.apiKey    = m_apiKeyEdit->text();
    m.modelName = m_modelEdit->text();
    m.validated = false;
    return m;
}

void ModelEditDialog::onProviderChanged(const QString &provider)
{
    m_endpointEdit->setPlaceholderText(defaultEndpoint(provider));
    m_modelEdit->setPlaceholderText(defaultModel(provider));
    if (provider == "Ollama")
        m_apiKeyEdit->setPlaceholderText("(not required)");
    else
        m_apiKeyEdit->setPlaceholderText("sk-...");
}

QString ModelEditDialog::defaultEndpoint(const QString &provider) const
{
    if (provider == "Anthropic") return "https://api.anthropic.com/v1/messages";
    if (provider == "OpenAI")    return "https://api.openai.com/v1/chat/completions";
    if (provider == "Google")    return "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions";
    if (provider == "Ollama")    return "http://localhost:11434/api/generate";
    return "https://api.openai.com/v1/chat/completions";
}

QString ModelEditDialog::defaultModel(const QString &provider) const
{
    if (provider == "Anthropic") return "claude-sonnet-4-6";
    if (provider == "OpenAI")    return "gpt-4o";
    if (provider == "Google")    return "gemini-2.0-flash";
    if (provider == "Ollama")    return "llama3";
    return "gpt-4o";
}
