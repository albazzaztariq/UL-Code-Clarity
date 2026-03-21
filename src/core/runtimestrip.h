#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QMap>
#include <QVector>

class RuntimeStrip : public QWidget {
    Q_OBJECT

public:
    explicit RuntimeStrip(QWidget* parent = nullptr);

    void setLanguage(const QString& langDisplay);
    void toggle();
    void show();
    void hide();
    void applyTheme(bool isDark);

private:
    struct RuntimeRow {
        QString label;
        QString value;         // static display text (empty if tunable)
        QStringList options;   // non-empty if this is a tunable combo
        QString helpTitle;
        QString helpBody;
    };

    void rebuild();
    void addItem(QHBoxLayout* layout, const RuntimeRow& row);
    void addSeparator(QHBoxLayout* layout);
    void showHelpTooltip(QWidget* anchor, const QString& title, const QString& body);

    QHBoxLayout* m_layout;
    QString      m_currentLang;
    bool         m_visible = true;
    bool         m_isDark  = true;

    QMap<QString, QVector<RuntimeRow>> m_runtimeData;
};
