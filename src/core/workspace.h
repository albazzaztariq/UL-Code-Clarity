#pragma once
// workspace.h — WorkspaceManager; implementation merged into mainwindow.cpp

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSettings>

class WorkspaceManager : public QObject {
    Q_OBJECT
public:
    explicit WorkspaceManager(QObject *parent = nullptr);

    QString savedWorkspacePath() const;
    void setWorkspacePath(const QString &path);

    void saveOpenTabs(const QStringList &paths);
    QStringList savedOpenTabs() const;

    void saveActiveTabIndex(int index);
    int savedActiveTabIndex() const;

    void clearWorkspace();

signals:
    void workspaceChanged(const QString &path);

private:
    QSettings m_settings;
};
