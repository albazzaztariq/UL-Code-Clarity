#ifndef MODELCONFIG_H
#define MODELCONFIG_H

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSettings>

// Holds a saved model configuration
struct ModelConfig {
    QString provider;    // "Anthropic", "OpenAI", "Google", "Ollama", "Custom"
    QString endpoint;
    QString apiKey;
    QString modelName;
};

// Dialog for configuring AI model connection
class ModelConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit ModelConfigDialog(QWidget *parent = nullptr);

    // Load saved config into fields
    void loadFromSettings();

    // Save fields to settings
    void saveToSettings();

    // Current values
    ModelConfig currentConfig() const;

private slots:
    void onProviderChanged(const QString &provider);
    void onSaveClicked();

private:
    QComboBox *m_providerCombo;
    QLineEdit *m_endpointEdit;
    QLineEdit *m_apiKeyEdit;
    QLineEdit *m_modelNameEdit;
    QPushButton *m_saveButton;
    QPushButton *m_cancelButton;
    QLabel *m_apiKeyLabel;   // form row label — hidden for Ollama
    QLabel *m_ollamaNote;    // shown only when Ollama is selected
    QFormLayout *m_form;     // kept to show/hide rows

    void setDefaultsForProvider(const QString &provider);
};

#endif // MODELCONFIG_H
