#pragma once

#include <QObject>
#include <QString>

// AIPermissions — enforces workspace-scoped file access for AI operations.
//
// Rules:
//  1. The file path MUST be under the current workspace root. Paths outside
//     the workspace (including /bin, /system32, AppData, etc.) are silently denied.
//  2. If "disable all permission prompts" is enabled in QSettings
//     (permissions/disablePrompts = true), access is granted automatically.
//  3. Otherwise a modal dialog is shown: Allow / Deny / Go to Settings.
//
// Usage:
//   AIPermissions perm;
//   perm.setWorkspaceRoot(m_fileTree->rootPath());
//   if (perm.requestAccess(filePath, AIPermissions::Read, parentWidget))
//       // proceed

class QWidget;

class AIPermissions : public QObject {
    Q_OBJECT

public:
    enum Operation { Read, Write };

    explicit AIPermissions(QObject *parent = nullptr);

    // Set workspace root — only paths under this root are ever permitted.
    void setWorkspaceRoot(const QString &root);
    QString workspaceRoot() const { return m_workspaceRoot; }

    // Request permission for the AI to access filePath.
    // Returns true if access is granted, false if denied.
    // parent is used as the parent window for any prompt dialog.
    bool requestAccess(const QString &filePath, Operation op, QWidget *parent = nullptr);

    // True if the path is under the workspace root
    static bool isUnderWorkspace(const QString &filePath, const QString &workspaceRoot);

private:
    QString m_workspaceRoot;
};

// ── PermissionGuard ───────────────────────────────────────────────────────
// Legacy static helper — workspace-scoped file access guard with modal prompt.
// Prefer AIPermissions for new code.
//
// Rules:
//  1. filePath MUST be under workspaceRoot (canonical path comparison).
//  2. If QSettings permissions/disablePrompts == true, access is granted silently.
//  3. Otherwise a modal dialog: Allow / Deny / Go to Settings.

class PermissionGuard {
public:
    // True only when filePath is a descendant of workspaceRoot.
    static bool canAccess(const QString &filePath, const QString &workspaceRoot);

    // Reads QSettings("CodeClarity","CodeClarity") permissions/disablePrompts.
    static bool shouldPrompt();

    // Show a prompt dialog "The AI wants to [action] [filePath]. Allow?"
    // Returns true if the user clicks Allow (or prompts are disabled).
    static bool requestPermission(QWidget *parent,
                                  const QString &action,
                                  const QString &filePath);
};
