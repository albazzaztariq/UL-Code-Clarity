#include "core/permissions.h"
#include "core/settingspanel.h"

#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QApplication>

// ── canAccess ─────────────────────────────────────────────────────────────

bool PermissionGuard::canAccess(const QString &filePath, const QString &workspaceRoot)
{
    if (workspaceRoot.isEmpty()) return false;

    // Resolve to canonical (absolute, symlink-resolved) paths for safe comparison
    QString canonical = QFileInfo(filePath).canonicalFilePath();
    QString root      = QFileInfo(workspaceRoot).canonicalFilePath();

    if (canonical.isEmpty() || root.isEmpty()) {
        // File may not exist yet (write to new file). Fall back to string check.
        QString absFile = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
        QString absRoot = QDir::cleanPath(QFileInfo(workspaceRoot).absoluteFilePath());
        if (!absRoot.endsWith('/') && !absRoot.endsWith('\\'))
            absRoot += '/';
        return absFile.startsWith(absRoot, Qt::CaseInsensitive);
    }

    // Ensure root ends with separator so "/workspace2" doesn't match "/workspace"
    if (!root.endsWith('/') && !root.endsWith('\\'))
        root += '/';

    return canonical.startsWith(root, Qt::CaseInsensitive);
}

// ── shouldPrompt ──────────────────────────────────────────────────────────

bool PermissionGuard::shouldPrompt()
{
    QSettings s("CodeClarity", "CodeClarity");
    return !s.value("permissions/disablePrompts", false).toBool();
}

// ── requestPermission ─────────────────────────────────────────────────────

bool PermissionGuard::requestPermission(QWidget *parent,
                                        const QString &action,
                                        const QString &filePath)
{
    // If prompts are disabled, grant automatically
    if (!shouldPrompt())
        return true;

    // Build dialog
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

    // Icon + title row
    auto *titleRow = new QHBoxLayout;
    auto *iconLabel = new QLabel(QString::fromUtf8("\xf0\x9f\x94\x92")); // lock emoji
    iconLabel->setStyleSheet("font-size: 20px;");
    titleRow->addWidget(iconLabel);

    auto *titleLabel = new QLabel("AI File Access Request");
    titleLabel->setStyleSheet("font-size: 13px; font-weight: 700; color: #89b4fa;");
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    layout->addLayout(titleRow);

    // Message
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

    // Button row
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
        // Open settings panel on Permissions tab (tab index 3)
        SettingsPanel panel(parent);
        panel.openToTab(3);
        panel.exec();
        // After settings dialog closes, re-evaluate (user may have disabled prompts)
        if (!shouldPrompt()) granted = true;
    });

    dlg.exec();
    return granted;
}
