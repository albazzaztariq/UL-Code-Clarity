#include "core/aipermissions.h"

#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QPushButton>

AIPermissions::AIPermissions(QObject *parent)
    : QObject(parent)
{
}

void AIPermissions::setWorkspaceRoot(const QString &root)
{
    m_workspaceRoot = QDir::cleanPath(root);
}

bool AIPermissions::isUnderWorkspace(const QString &filePath, const QString &workspaceRoot)
{
    if (workspaceRoot.isEmpty()) return false;

    QString cleanFile = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
    QString cleanRoot = QDir::cleanPath(workspaceRoot);

    // Must start with root + separator (or equal root itself)
    return cleanFile.startsWith(cleanRoot + "/") || cleanFile == cleanRoot;
}

bool AIPermissions::requestAccess(const QString &filePath, Operation op, QWidget *parent)
{
    // Rule 1: path must be under workspace root — hard deny if not
    if (!isUnderWorkspace(filePath, m_workspaceRoot)) {
        return false;
    }

    // Rule 2: if prompts are disabled, allow immediately
    QSettings s("CodeClarity", "CodeClarity");
    if (s.value("permissions/disablePrompts", false).toBool()) {
        return true;
    }

    // Rule 3: show prompt
    QString opStr = (op == Read) ? "read" : "write";
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
        // Emit a signal or post a queued event — for now just deny so the caller
        // can tell the user to open Settings manually. The Settings button text
        // itself is the navigation cue.
        return false;
    }

    return (clicked == allowBtn);
}
