#include "editor/filetree.h"
#include "core/theme.h"

#include <QHeaderView>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

// ─── CodeFileProxyModel ──────────────────────────────────────────────────────

const QSet<QString>& CodeFileProxyModel::codeExtensions() {
    static const QSet<QString> exts = {
        "py","c","cpp","h","ul","js","ts","rs","java","go","rb",
        "txt","md","json","yaml","yml","toml","xml","html","css","hpp","cxx","cc"
    };
    return exts;
}

CodeFileProxyModel::CodeFileProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{}

void CodeFileProxyModel::setRootPath(const QString& root) {
    m_rootPath = root;
    m_fsModel = qobject_cast<QFileSystemModel*>(sourceModel());
    invalidateFilter();
}

static bool isBuildArtifactDir(const QString& name) {
    // Exact name matches for known build directories
    static const QSet<QString> buildDirs = {
        "build", "Build", "cmake-build-debug", "cmake-build-release",
        ".cmake", "__pycache__", ".git", ".vscode", ".idea", "node_modules",
        "dist", "out", "target", "obj", "bin", ".obj"
    };
    if (buildDirs.contains(name)) return true;

    // Suffix patterns for generated/artifact directories
    if (name.endsWith("_autogen"))  return true;
    if (name.endsWith("_autogen/")) return true;
    if (name == "CMakeFiles")       return true;

    // Detect hash-like directory names (8+ hex chars, no vowels/spaces)
    // e.g. TAC5DWH4SE, MJFHPCJYR3, 3HAKN7MTYQ, EWIEGA46WW
    if (name.length() >= 8 && name.length() <= 12) {
        bool allAlnumOrUpper = true;
        for (QChar c : name) {
            if (!c.isLetterOrNumber()) { allAlnumOrUpper = false; break; }
        }
        int digitCount = 0;
        for (QChar c : name) { if (c.isDigit()) digitCount++; }
        // Hash dirs have both letters and digits, all uppercase
        if (allAlnumOrUpper && digitCount >= 2 && name == name.toUpper())
            return true;
    }

    return false;
}

static bool isBuildArtifactFile(const QString& name) {
    if (name.startsWith("moc_"))    return true;
    if (name.startsWith("qrc_"))    return true;
    if (name.startsWith("ui_"))     return true;
    if (name == "mocs_compilation.cpp") return true;
    if (name == "CMakeCXXCompilerId.cpp") return true;
    if (name == "CMakeCCompilerId.c")     return true;
    if (name.endsWith(".moc"))      return true;
    return false;
}

bool CodeFileProxyModel::dirContainsCodeFiles(const QString& dirPath, int depth) const {
    if (depth > 3) return false;
    QDir dir(dirPath);
    // Check files directly in this dir (skip generated files)
    const auto entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& fi : entries) {
        if (isBuildArtifactFile(fi.fileName())) continue;
        if (CodeFileProxyModel::codeExtensions().contains(fi.suffix().toLower()))
            return true;
    }
    // Recurse into subdirs (skip artifact directories)
    const auto subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& fi : subdirs) {
        if (isBuildArtifactDir(fi.fileName())) continue;
        if (dirContainsCodeFiles(fi.absoluteFilePath(), depth + 1))
            return true;
    }
    return false;
}

bool CodeFileProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    if (!m_fsModel) return true;

    QModelIndex idx = m_fsModel->index(sourceRow, 0, sourceParent);
    if (!idx.isValid()) return false;

    QString path = m_fsModel->filePath(idx);
    QString name = QFileInfo(path).fileName();

    // Calculate depth from root (root itself = depth 0, children = depth 1, etc.)
    int depth = 0;
    if (!m_rootPath.isEmpty() && path.startsWith(m_rootPath)) {
        QString rel = path.mid(m_rootPath.length());
        // Count separators
        for (QChar c : rel) {
            if (c == '/' || c == '\\') depth++;
        }
    }

    if (m_fsModel->isDir(idx)) {
        // Exclude known build artifact directories at any depth
        if (isBuildArtifactDir(name)) return false;
        // Only show dirs within 3 levels that contain code files
        if (depth > 3) return false;
        return dirContainsCodeFiles(path, 1);
    } else {
        // Exclude generated source files
        if (isBuildArtifactFile(name)) return false;
        // Only show code files
        QString ext = QFileInfo(path).suffix().toLower();
        return codeExtensions().contains(ext);
    }
}

FileTreeWidget::FileTreeWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setStyleSheet("background: #2a2a3c; border-right: 1px solid #45475a;");

    // Header: "Explorer" + collapse button
    m_header = new QWidget;
    auto* headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(12, 8, 12, 8);

    m_headerLabel = new QLabel("EXPLORER");
    m_headerLabel->setStyleSheet(
        "QLabel { color: #a6adc8; font-size: 10px; font-weight: 700;"
        " letter-spacing: 1px; }");
    headerLayout->addWidget(m_headerLabel);

    headerLayout->addStretch();

    m_collapseBtn = new QPushButton(QString::fromUtf8("\xe2\x97\x80"));  // ◀ collapse (pointing left = collapse)
    m_collapseBtn->setFixedSize(22, 22);
    m_collapseBtn->setToolTip("Collapse Explorer");
    m_collapseBtn->setStyleSheet(
        "QPushButton { color: #a6adc8; background: transparent;"
        " font-size: 14px; border: none; padding: 0; }"
        "QPushButton:hover { color: #cdd6f4; background: #3c3c54; border-radius: 4px; }");
    headerLayout->addWidget(m_collapseBtn);

    layout->addWidget(m_header);

    // Stacked widget: empty state vs tree state
    m_stack = new QStackedWidget;

    // ── Empty state page ──
    m_emptyPage = new QWidget;
    m_emptyPage->setStyleSheet("background: #2a2a3c;");
    auto* emptyLayout = new QVBoxLayout(m_emptyPage);
    emptyLayout->setContentsMargins(16, 40, 16, 16);
    emptyLayout->setSpacing(12);

    auto* emptyLabel = new QLabel("No workspace open");
    emptyLabel->setStyleSheet(
        "QLabel { color: #a6adc8; font-size: 12px; }");
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyLabel);

    m_addWorkspaceBtn = new QPushButton("Add Workspace");
    m_addWorkspaceBtn->setStyleSheet(
        "QPushButton {"
        "  color: #cdd6f4; background: #45475a; border: 1px solid #585b70;"
        "  border-radius: 4px; padding: 6px 16px; font-size: 11px; font-weight: 600;"
        "}"
        "QPushButton:hover { background: #585b70; }");
    m_addWorkspaceBtn->setCursor(Qt::PointingHandCursor);
    emptyLayout->addWidget(m_addWorkspaceBtn, 0, Qt::AlignCenter);

    emptyLayout->addStretch();
    m_stack->addWidget(m_emptyPage);  // index 0

    // ── Tree state page ──
    m_treePage = new QWidget;
    m_treePage->setStyleSheet("background: #2a2a3c;");
    auto* treeLayout = new QVBoxLayout(m_treePage);
    treeLayout->setContentsMargins(0, 0, 0, 0);
    treeLayout->setSpacing(0);

    m_fsModel = new QFileSystemModel(this);
    m_fsModel->setReadOnly(true);
    m_fsModel->setNameFilterDisables(false);

    m_proxyModel = new CodeFileProxyModel(this);
    m_proxyModel->setSourceModel(m_fsModel);
    m_proxyModel->setDynamicSortFilter(true);

    m_multiModel = new QStandardItemModel(this);

    m_treeView = new QTreeView;
    m_treeView->setModel(m_proxyModel);
    m_treeView->setHeaderHidden(true);
    m_treeView->setAnimated(true);
    m_treeView->setIndentation(16);
    m_treeView->setExpandsOnDoubleClick(true);

    // Only show name column
    m_treeView->hideColumn(1);  // Size
    m_treeView->hideColumn(2);  // Type
    m_treeView->hideColumn(3);  // Date Modified

    m_treeView->setStyleSheet(
        "QTreeView { background: #2a2a3c; color: #a6adc8; border: none; font-size: 12px; }"
        "QTreeView::item { padding: 3px 6px; min-height: 24px; }"
        "QTreeView::item:hover { background: #313244; }"
        "QTreeView::item:selected { background: transparent; color: #cdd6f4;"
        " border-left: 2px solid #89b4fa; }"
        "QTreeView::item:selected:hover { background: #313244; }"
        "QTreeView::branch { background: #2a2a3c; color: #6c7086; }"
        // Closed folder — right-pointing triangle
        "QTreeView::branch:has-children:!has-siblings:closed,"
        "QTreeView::branch:closed:has-children:has-siblings {"
        "  border-image: none; image: none; }"
        // Open folder — down-pointing triangle
        "QTreeView::branch:open:has-children:!has-siblings,"
        "QTreeView::branch:open:has-children:has-siblings {"
        "  border-image: none; image: none; }"
    );

    treeLayout->addWidget(m_treeView, 1);
    m_stack->addWidget(m_treePage);  // index 1

    layout->addWidget(m_stack, 1);

    // Start in empty state
    m_stack->setCurrentIndex(0);

    // Connections
    connect(m_collapseBtn, &QPushButton::clicked, this, [this]() {
        m_collapsed = !m_collapsed;
        if (m_collapsed) {
            collapse();
        } else {
            expand();
        }
    });

    connect(m_addWorkspaceBtn, &QPushButton::clicked, this, [this]() {
        emit addWorkspaceRequested();
    });

    // Single-click: open file, toggle folder expand/collapse
    connect(m_treeView, &QTreeView::clicked, this, [this](const QModelIndex& proxyIndex) {
        QModelIndex srcIndex = m_proxyModel->mapToSource(proxyIndex);
        if (!srcIndex.isValid()) return;

        if (m_fsModel->isDir(srcIndex)) {
            if (m_treeView->isExpanded(proxyIndex))
                m_treeView->collapse(proxyIndex);
            else
                m_treeView->expand(proxyIndex);
        } else {
            QString path = m_fsModel->filePath(srcIndex);
            if (!path.isEmpty())
                emit fileSelected(path);
        }
    });

    // Double-click on folder also expands (Qt's built-in expandsOnDoubleClick handles it,
    // but we also emit file for files to be safe)
    connect(m_treeView, &QTreeView::doubleClicked, this, [this](const QModelIndex& proxyIndex) {
        QModelIndex srcIndex = m_proxyModel->mapToSource(proxyIndex);
        if (!srcIndex.isValid()) return;
        if (!m_fsModel->isDir(srcIndex)) {
            QString path = m_fsModel->filePath(srcIndex);
            if (!path.isEmpty())
                emit fileSelected(path);
        }
    });
}

void FileTreeWidget::setRootFolder(const QString& path)
{
    if (path.isEmpty()) {
        showEmptyState();
        return;
    }
    m_workspacePaths.clear();
    m_workspacePaths << path;
    m_proxyModel->setRootPath(path);
    m_fsModel->setRootPath(path);
    QModelIndex srcRoot = m_fsModel->index(path);
    m_treeView->setRootIndex(m_proxyModel->mapFromSource(srcRoot));
    m_treeView->hideColumn(1);
    m_treeView->hideColumn(2);
    m_treeView->hideColumn(3);
    showTreeState();
}

void FileTreeWidget::addWorkspaceFolder(const QString& path)
{
    if (m_workspacePaths.contains(path))
        return;

    if (m_workspacePaths.isEmpty()) {
        setRootFolder(path);
        return;
    }

    m_workspacePaths << path;

    // For simplicity, use the first workspace as root
    QString root = m_workspacePaths.first();
    m_proxyModel->setRootPath(root);
    m_fsModel->setRootPath(root);
    QModelIndex srcRoot = m_fsModel->index(root);
    m_treeView->setRootIndex(m_proxyModel->mapFromSource(srcRoot));
    m_treeView->hideColumn(1);
    m_treeView->hideColumn(2);
    m_treeView->hideColumn(3);
    showTreeState();
}

void FileTreeWidget::removeWorkspaceFolder(const QString& path)
{
    m_workspacePaths.removeAll(path);
    if (m_workspacePaths.isEmpty()) {
        showEmptyState();
    } else {
        setRootFolder(m_workspacePaths.first());
    }
}

QString FileTreeWidget::rootPath() const
{
    return m_workspacePaths.isEmpty() ? QString() : m_workspacePaths.first();
}

void FileTreeWidget::showEmptyState()
{
    m_stack->setCurrentIndex(0);
}

void FileTreeWidget::showTreeState()
{
    m_stack->setCurrentIndex(1);
}

void FileTreeWidget::collapse()
{
    m_collapsed = true;
    m_stack->hide();
    m_headerLabel->hide();
    m_collapseBtn->setText(QString::fromUtf8("\xe2\x96\xb6"));  // ▶ right-pointing = expand
    m_collapseBtn->setToolTip("Expand Explorer");
    setMaximumWidth(32);
    setMinimumWidth(32);
}

void FileTreeWidget::expand()
{
    m_collapsed = false;
    m_stack->show();
    m_headerLabel->show();
    m_collapseBtn->setText(QString::fromUtf8("\xe2\x97\x80"));  // ◀ left-pointing = collapse
    m_collapseBtn->setToolTip("Collapse Explorer");
    setMaximumWidth(400);
    setMinimumWidth(140);
}

void FileTreeWidget::rebuildTreeModel()
{
    // Reserved for future multi-root tree implementation
}

void FileTreeWidget::applyTheme(bool isDark)
{
    if (isDark) {
        setStyleSheet("background: #2a2a3c; border-right: 1px solid #45475a;");
        m_headerLabel->setStyleSheet(
            "QLabel { color: #a6adc8; font-size: 10px; font-weight: 700;"
            " letter-spacing: 1px; }");
        m_collapseBtn->setStyleSheet(
            "QPushButton { color: #a6adc8; background: transparent;"
            " font-size: 14px; border: none; padding: 0; }"
            "QPushButton:hover { color: #cdd6f4; background: #3c3c54; border-radius: 4px; }");
        m_emptyPage->setStyleSheet("background: #2a2a3c;");
        m_emptyPage->findChild<QLabel*>()->setStyleSheet(
            "QLabel { color: #a6adc8; font-size: 12px; }");
        m_addWorkspaceBtn->setStyleSheet(
            "QPushButton {"
            "  color: #cdd6f4; background: #45475a; border: 1px solid #585b70;"
            "  border-radius: 4px; padding: 6px 16px; font-size: 11px; font-weight: 600;"
            "}"
            "QPushButton:hover { background: #585b70; }");
        m_treePage->setStyleSheet("background: #2a2a3c;");
        m_treeView->setStyleSheet(
            "QTreeView { background: #2a2a3c; color: #a6adc8; border: none; font-size: 12px; }"
            "QTreeView::item { padding: 3px 6px; min-height: 24px; }"
            "QTreeView::item:hover { background: #313244; }"
            "QTreeView::item:selected { background: transparent; color: #cdd6f4;"
            " border-left: 2px solid #89b4fa; }"
            "QTreeView::item:selected:hover { background: #313244; }"
            "QTreeView::branch { background: #2a2a3c; color: #6c7086; }"
            "QTreeView::branch:has-children:!has-siblings:closed,"
            "QTreeView::branch:closed:has-children:has-siblings {"
            "  border-image: none; image: none; }"
            "QTreeView::branch:open:has-children:!has-siblings,"
            "QTreeView::branch:open:has-children:has-siblings {"
            "  border-image: none; image: none; }");
    } else {
        setStyleSheet("background: #f5f5f5; border-right: 1px solid #d0d0d0;");
        m_headerLabel->setStyleSheet(
            "QLabel { color: #666666; font-size: 10px; font-weight: 700;"
            " letter-spacing: 1px; background: transparent; }");
        m_collapseBtn->setStyleSheet(
            "QPushButton { color: #666666; background: transparent;"
            " font-size: 14px; border: none; padding: 0; }"
            "QPushButton:hover { color: #1e1e2e; background: #e0e0e0; border-radius: 4px; }");
        m_emptyPage->setStyleSheet("background: #f5f5f5;");
        m_emptyPage->findChild<QLabel*>()->setStyleSheet(
            "QLabel { color: #666666; font-size: 12px; }");
        m_addWorkspaceBtn->setStyleSheet(
            "QPushButton {"
            "  color: #1e1e2e; background: #e8e8e8; border: 1px solid #d0d0d0;"
            "  border-radius: 4px; padding: 6px 16px; font-size: 11px; font-weight: 600;"
            "}"
            "QPushButton:hover { background: #d8d8d8; }");
        m_treePage->setStyleSheet("background: #f5f5f5;");
        m_treeView->setStyleSheet(
            "QTreeView { background: #f5f5f5; color: #1e1e2e; border: none; font-size: 12px; }"
            "QTreeView::item { padding: 3px 6px; min-height: 24px; }"
            "QTreeView::item:hover { background: #e8e8e8; }"
            "QTreeView::item:selected { background: transparent; color: #2563eb;"
            " border-left: 2px solid #2563eb; }"
            "QTreeView::item:selected:hover { background: #e8e8e8; }"
            "QTreeView::branch { background: #f5f5f5; color: #999999; }"
            "QTreeView::branch:has-children:!has-siblings:closed,"
            "QTreeView::branch:closed:has-children:has-siblings {"
            "  border-image: none; image: none; }"
            "QTreeView::branch:open:has-children:!has-siblings,"
            "QTreeView::branch:open:has-children:has-siblings {"
            "  border-image: none; image: none; }");
    }
}
