#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QListWidget>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>
#include <QString>
#include "core/cvemonitor.h"

// Represents a single saved model configuration
struct SavedModel {
    QString provider;
    QString endpoint;
    QString apiKey;
    QString modelName;
    bool    validated = false;

    bool operator==(const SavedModel &o) const {
        return provider == o.provider && modelName == o.modelName
            && endpoint == o.endpoint;
    }
};

// SettingsPanel — multi-tab settings dialog
//
// Tabs:
//   General    — theme toggle (dark/light), editor font size and family
//   Models     — list of all configured models; add/edit/remove; test connection
//   AI Behavior — web search toggle, custom system prompt
//   Permissions — disable-all-prompts checkbox (workspace-scoped only)

class SettingsPanel : public QDialog {
    Q_OBJECT

public:
    explicit SettingsPanel(QWidget *parent = nullptr);

    // Reload and repopulate the model list (call after external changes)
    void refreshModelList();

    // Open the dialog and switch to the given tab index (0=General, 1=Models, 2=AI, 3=Permissions)
    void openToTab(int index);

    // Static helpers for reading/writing the model list
    static QList<SavedModel> loadModels();
    static void saveModels(const QList<SavedModel> &models);
    static void addOrUpdateModel(const SavedModel &m);

signals:
    // Emitted when the model list changes so AIChatPanel can refresh its dropdown
    void modelsChanged();
    // Emitted when Tools menu item visibility changes
    void menuVisibilityChanged();
    // Emitted when news ticker settings change
    void tickerSettingsChanged();
    // Emitted when CVE monitoring settings change
    void cveSettingsChanged();

private slots:
    void onAddModel();
    void onEditModel();
    void onRemoveModel();
    void onTestModel();
    void onTestReply(QNetworkReply *reply);
    void onSaveClicked();

private:
    void buildGeneralTab(QTabWidget *tabs);
    void buildModelsTab(QTabWidget *tabs);
    void buildAIBehaviorTab(QTabWidget *tabs);
    void buildPermissionsTab(QTabWidget *tabs);
    void buildMenuCustomizationTab(QTabWidget *tabs);
    void buildNewsTickerTab(QTabWidget *tabs);
    void buildSecurityTab(QTabWidget *tabs);

    void loadSettings();
    void applySettings();

    QTabWidget *m_tabs          = nullptr;

    // General
    QComboBox  *m_themeCombo    = nullptr;
    QLineEdit  *m_fontSizeEdit  = nullptr;  // custom spin: number field
    QComboBox  *m_fontFamilyCombo = nullptr;

    // Models
    QListWidget *m_modelList    = nullptr;
    QPushButton *m_addBtn       = nullptr;
    QPushButton *m_editBtn      = nullptr;
    QPushButton *m_removeBtn    = nullptr;
    QPushButton *m_testBtn      = nullptr;
    QLabel      *m_testStatus   = nullptr;

    // AI Behavior
    QCheckBox  *m_webSearchChk  = nullptr;
    QTextEdit  *m_systemPromptEdit = nullptr;

    // Permissions
    QCheckBox  *m_disablePromptsChk = nullptr;

    // Menu Customization
    QMap<QString, QCheckBox*> m_menuVisCheckBoxes;

    // Security / CVE
    QCheckBox  *m_cveEnabledChk        = nullptr;
    QComboBox  *m_cveFreqCombo         = nullptr;

    // News Ticker
    QCheckBox  *m_tickerEnabledChk     = nullptr;
    QCheckBox  *m_tickerSubProgramming = nullptr;
    QCheckBox  *m_tickerSubPython      = nullptr;
    QCheckBox  *m_tickerSubJavascript  = nullptr;
    QCheckBox  *m_tickerSubCProg       = nullptr;
    QCheckBox  *m_tickerSubCpp         = nullptr;
    QComboBox  *m_tickerPollCombo      = nullptr;

    // Network for test connection
    QNetworkAccessManager *m_nam = nullptr;

    // Cached model list (synced with list widget)
    QList<SavedModel> m_models;
};


// ── Inline model edit dialog ───────────────────────────────────────────────

class ModelEditDialog : public QDialog {
    Q_OBJECT
public:
    explicit ModelEditDialog(QWidget *parent = nullptr, const SavedModel &existing = {});
    SavedModel model() const;

private:
    QComboBox *m_providerCombo;
    QLineEdit *m_endpointEdit;
    QLineEdit *m_apiKeyEdit;
    QLineEdit *m_modelEdit;

    void onProviderChanged(const QString &provider);
    QString defaultEndpoint(const QString &provider) const;
    QString defaultModel(const QString &provider) const;
};
