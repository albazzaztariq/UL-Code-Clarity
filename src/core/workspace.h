#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSettings>

// Manages workspace persistence: folder path and open tab list
class WorkspaceManager : public QObject {
    Q_OBJECT
public:
    explicit WorkspaceManager(QObject *parent = nullptr);

    // Returns saved workspace path, or empty string if none saved
    QString savedWorkspacePath() const;

    // Call when the user picks a workspace folder
    void setWorkspacePath(const QString &path);

    // Save the list of currently open file paths
    void saveOpenTabs(const QStringList &paths);

    // Restore previously open tabs
    QStringList savedOpenTabs() const;

    // Save which tab index was active
    void saveActiveTabIndex(int index);
    int savedActiveTabIndex() const;

    // Clear all saved workspace state
    void clearWorkspace();

signals:
    void workspaceChanged(const QString &path);

private:
    QSettings m_settings;
};
