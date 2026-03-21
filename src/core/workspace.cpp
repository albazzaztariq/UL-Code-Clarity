#include "core/workspace.h"

WorkspaceManager::WorkspaceManager(QObject *parent)
    : QObject(parent)
    , m_settings("CodeClarity", "CodeClarity")
{
}

QString WorkspaceManager::savedWorkspacePath() const
{
    return m_settings.value("workspace/path").toString();
}

void WorkspaceManager::setWorkspacePath(const QString &path)
{
    m_settings.setValue("workspace/path", path);
    emit workspaceChanged(path);
}

void WorkspaceManager::saveOpenTabs(const QStringList &paths)
{
    m_settings.setValue("workspace/openTabs", paths);
}

QStringList WorkspaceManager::savedOpenTabs() const
{
    return m_settings.value("workspace/openTabs").toStringList();
}

void WorkspaceManager::saveActiveTabIndex(int index)
{
    m_settings.setValue("workspace/activeTab", index);
}

int WorkspaceManager::savedActiveTabIndex() const
{
    return m_settings.value("workspace/activeTab", 0).toInt();
}

void WorkspaceManager::clearWorkspace()
{
    m_settings.remove("workspace/path");
    m_settings.remove("workspace/openTabs");
    m_settings.remove("workspace/activeTab");
}
