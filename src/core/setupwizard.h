#pragma once

#include <QDialog>
#include <QStackedWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// SetupWizard -- shown on first launch when ai/provider is not configured.
// Closeable/skippable. AI Chat shows a banner if no model is configured.
//
// Pages:
//   0 -- Welcome
//   1 -- Provider configuration (provider, endpoint, API key, model)
//   2 -- Test Connection
//   3 -- Setup Complete
//
// Saves settings to QSettings("CodeClarity","CodeClarity") under ai/* keys.

class SetupWizard : public QDialog {
    Q_OBJECT

public:
    explicit SetupWizard(QWidget *parent = nullptr);

private slots:
    void onNextClicked();
    void onBackClicked();
    void onProviderChanged(const QString &provider);
    void onTestClicked();
    void onTestReply(QNetworkReply *reply);

private:
    void buildWelcomePage();
    void buildProviderPage();
    void buildTestPage();
    void buildDonePage();

    void goTo(int page);
    void updateNavButtons();
    void saveToSettings();
    QString defaultEndpoint(const QString &provider) const;
    QString defaultModel(const QString &provider) const;

    QStackedWidget *m_pages;
    QPushButton    *m_backBtn;
    QPushButton    *m_nextBtn;
    QPushButton    *m_finishBtn;

    // Page 1 widgets
    QComboBox  *m_providerCombo;
    QLineEdit  *m_endpointEdit;
    QLineEdit  *m_apiKeyEdit;
    QLineEdit  *m_modelEdit;
    QLabel     *m_apiKeyLabel;   // form row label -- hidden for Ollama
    QLabel     *m_ollamaNote;    // shown only when Ollama is selected

    // Page 2 widgets
    QLabel      *m_testStatus;
    QPushButton *m_testBtn;
    QPushButton *m_tryAgainBtn;
    QLabel      *m_testResponse;
    bool         m_testPassed = false;

    // Network for test connection
    QNetworkAccessManager *m_nam;

    int m_currentPage = 0;
    static const int PAGE_COUNT = 4;
};
