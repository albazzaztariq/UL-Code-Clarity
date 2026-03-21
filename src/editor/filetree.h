#pragma once

#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStringList>
#include <QSet>

// Proxy model that filters the file tree to show only code-containing folders
// up to 3 levels deep from the workspace root.
class CodeFileProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit CodeFileProxyModel(QObject* parent = nullptr);
    void setRootPath(const QString& root);
    static const QSet<QString>& codeExtensions();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    bool dirContainsCodeFiles(const QString& dirPath, int depth) const;
    QString m_rootPath;
    QFileSystemModel* m_fsModel = nullptr;
};

class FileTreeWidget : public QWidget {
    Q_OBJECT

public:
    explicit FileTreeWidget(QWidget* parent = nullptr);

    void setRootFolder(const QString& path);
    void addWorkspaceFolder(const QString& path);
    void removeWorkspaceFolder(const QString& path);
    QStringList workspacePaths() const { return m_workspacePaths; }
    QString rootPath() const;
    void collapse();
    void expand();
    void applyTheme(bool isDark);

signals:
    void fileSelected(const QString& filePath);
    void addWorkspaceRequested();

private:
    void rebuildTreeModel();
    void showEmptyState();
    void showTreeState();

    QTreeView*            m_treeView;
    QFileSystemModel*     m_fsModel;
    CodeFileProxyModel*   m_proxyModel;
    QStandardItemModel*   m_multiModel;
    QWidget*              m_header;
    QPushButton*          m_collapseBtn;
    QLabel*               m_headerLabel;

    // Empty state widgets
    QStackedWidget*       m_stack;
    QWidget*              m_emptyPage;
    QWidget*              m_treePage;
    QPushButton*          m_addWorkspaceBtn;

    QStringList           m_workspacePaths;
    bool                  m_collapsed = false;
};
