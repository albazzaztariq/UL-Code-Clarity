#include "core/aipermissions.h"

#include <QSettings>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QPushButton>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QApplication>
#include "core/settingspanel.h"

AIPermissions::AIPermissions(QObject *parent)
    : QObject(parent)
{
}

void AIPermissions::setWorkspaceRoot(const QString &root)
{
    qDebug() << "[DEBUG] AIPermissions::setWorkspaceRoot" << root;
    m_workspaceRoot = QDir::cleanPath(root);
}

bool AIPermissions::isUnderWorkspace(const QString &filePath, const QString &workspaceRoot)
{
    qDebug() << "[DEBUG] AIPermissions::isUnderWorkspace" << filePath << workspaceRoot;
    if (workspaceRoot.isEmpty()) return false;

    QString cleanFile = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
    QString cleanRoot = QDir::cleanPath(workspaceRoot);

    // Must start with root + separator (or equal root itself)
    return cleanFile.startsWith(cleanRoot + "/") || cleanFile == cleanRoot;
}

bool AIPermissions::requestAccess(const QString &filePath, Operation op, QWidget *parent)
{
    const QString opStr = (op == Read) ? "read" : "write";
    qDebug() << "[DEBUG] AIPermissions::requestAccess" << opStr << filePath;

    // Rule 1: path must be under workspace root — hard deny if not
    if (!isUnderWorkspace(filePath, m_workspaceRoot)) {
        qDebug() << "[DEBUG] AIPermissions::requestAccess denied: outside workspace";
        return false;
    }

    // Rule 2: if prompts are disabled, allow immediately
    QSettings s("CodeClarity", "CodeClarity");
    if (s.value("permissions/disablePrompts", false).toBool()) {
        qDebug() << "[DEBUG] AIPermissions::requestAccess allow: prompts disabled";
        return true;
    }

    // Rule 3: show prompt
    QString fileName = QFileInfo(filePath).fileName();

    QMessageBox dlg(parent);
    dlg.setWindowTitle("AI File Access");
    dlg.setText(QString("The AI wants to <b>%1</b> <code>%2</code>.<br><br>"
                        "Allow this action?")
                .arg(opStr, fileName));
    dlg.setIcon(QMessageBox::Question);

    QPushButton *allowBtn  = dlg.addButton("Allow",  QMessageBox::AcceptRole);
    QPushButton *denyBtn   = dlg.addButton("Deny",   QMessageBox::RejectRole);
    QPushButton *settingsBtn = dlg.addButton("Go to Settings \342\206\222 Permissions",
                                             QMessageBox::HelpRole);
    Q_UNUSED(denyBtn);
    dlg.setDefaultButton(allowBtn);

    dlg.setInformativeText(
        "Go to Settings \342\206\222 Permissions to disable these prompts.");

    dlg.exec();

    QAbstractButton *clicked = dlg.clickedButton();

    if (clicked == settingsBtn) {
        qDebug() << "[DEBUG] AIPermissions::requestAccess settings requested";
        // Emit a signal or post a queued event — for now just deny so the caller
        // can tell the user to open Settings manually. The Settings button text
        // itself is the navigation cue.
        return false;
    }

    qDebug() << "[DEBUG] AIPermissions::requestAccess decision" << (clicked == allowBtn);
    return (clicked == allowBtn);
}

// ── PermissionGuard ───────────────────────────────────────────────────────

bool PermissionGuard::canAccess(const QString &filePath, const QString &workspaceRoot)
{
    qDebug() << "[DEBUG] PermissionGuard::canAccess" << filePath << workspaceRoot;
    if (workspaceRoot.isEmpty()) return false;

    QString canonical = QFileInfo(filePath).canonicalFilePath();
    QString root      = QFileInfo(workspaceRoot).canonicalFilePath();

    if (canonical.isEmpty() || root.isEmpty()) {
        QString absFile = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
        QString absRoot = QDir::cleanPath(QFileInfo(workspaceRoot).absoluteFilePath());
        if (!absRoot.endsWith('/') && !absRoot.endsWith('\\'))
            absRoot += '/';
        return absFile.startsWith(absRoot, Qt::CaseInsensitive);
    }

    if (!root.endsWith('/') && !root.endsWith('\\'))
        root += '/';

    return canonical.startsWith(root, Qt::CaseInsensitive);
}

bool PermissionGuard::shouldPrompt()
{
    QSettings s("CodeClarity", "CodeClarity");
    bool prompt = !s.value("permissions/disablePrompts", false).toBool();
    qDebug() << "[DEBUG] PermissionGuard::shouldPrompt" << prompt;
    return prompt;
}

bool PermissionGuard::requestPermission(QWidget *parent,
                                        const QString &action,
                                        const QString &filePath)
{
    qDebug() << "[DEBUG] PermissionGuard::requestPermission" << action << filePath;
    if (!shouldPrompt())
        return true;

    QDialog dlg(parent);
    dlg.setWindowTitle("AI File Access Request");
    dlg.setModal(true);
    dlg.setMinimumWidth(380);

    dlg.setStyleSheet(
        "QDialog { background: #2a2a3c; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; font-size: 11px; }"
        "QPushButton { border-radius: 4px; padding: 6px 16px; font-size: 11px; }"
    );

    auto *layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(12);

    auto *titleRow = new QHBoxLayout;
    auto *iconLabel = new QLabel(QString::fromUtf8("\xf0\x9f\x94\x92"));
    iconLabel->setStyleSheet("font-size: 20px;");
    titleRow->addWidget(iconLabel);

    auto *titleLabel = new QLabel("AI File Access Request");
    titleLabel->setStyleSheet("font-size: 13px; font-weight: 700; color: #89b4fa;");
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    layout->addLayout(titleRow);

    QString shortPath = filePath;
    if (shortPath.length() > 60)
        shortPath = "..." + shortPath.right(57);

    auto *msgLabel = new QLabel(
        QString("The AI wants to <b>%1</b> the file:<br>"
                "<code style='color:#a6e3a1;'>%2</code>")
            .arg(action, shortPath.toHtmlEscaped())
    );
    msgLabel->setWordWrap(true);
    msgLabel->setTextFormat(Qt::RichText);
    layout->addWidget(msgLabel);

    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();

    auto *settingsBtn = new QPushButton("Go to Settings");
    settingsBtn->setStyleSheet(
        "QPushButton { background: #333348; color: #a6adc8; border: 1px solid #45475a; }"
        "QPushButton:hover { background: #3c3c54; }"
    );

    auto *denyBtn = new QPushButton("Deny");
    denyBtn->setStyleSheet(
        "QPushButton { background: #333348; color: #f38ba8; border: 1px solid #45475a; }"
        "QPushButton:hover { background: #3c3c54; }"
    );

    auto *allowBtn = new QPushButton("Allow");
    allowBtn->setDefault(true);
    allowBtn->setStyleSheet(
        "QPushButton { background: #89b4fa; color: #1e1e2e; font-weight: 600; border: none; }"
        "QPushButton:hover { background: #74a8e8; }"
    );

    btnRow->addWidget(settingsBtn);
    btnRow->addWidget(denyBtn);
    btnRow->addWidget(allowBtn);
    layout->addLayout(btnRow);

    bool granted = false;

    QObject::connect(allowBtn, &QPushButton::clicked, &dlg, [&]() {
        granted = true;
        dlg.accept();
    });
    QObject::connect(denyBtn, &QPushButton::clicked, &dlg, [&]() {
        granted = false;
        dlg.reject();
    });
    QObject::connect(settingsBtn, &QPushButton::clicked, &dlg, [&]() {
        dlg.reject();
        SettingsPanel panel(parent);
        panel.openToTab(3);
        panel.exec();
        if (!shouldPrompt()) granted = true;
    });

    dlg.exec();
    qDebug() << "[DEBUG] PermissionGuard::requestPermission granted" << granted;
    return granted;
}
