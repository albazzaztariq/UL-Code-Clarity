#pragma once

#include <QString>

class QWidget;

// PermissionGuard — enforces workspace-scoped file access for AI operations.
//
// Rules:
//  1. filePath MUST be under workspaceRoot. Paths outside (system dirs, AppData, etc.)
//     are always blocked.
//  2. If QSettings permissions/disablePrompts == true, access is granted silently.
//  3. Otherwise a modal dialog is shown: Allow / Deny / Go to Settings.

class PermissionGuard {
public:
    // True only when filePath is a descendant of workspaceRoot (canonical comparison).
    static bool canAccess(const QString &filePath, const QString &workspaceRoot);

    // Reads QSettings("CodeClarity","CodeClarity") permissions/disablePrompts.
    static bool shouldPrompt();

    // Show a prompt dialog "The AI wants to [action] [filePath]. Allow?"
    // Returns true if the user clicks Allow (or prompts are disabled).
    // Goes to Settings if user clicks "Go to Settings" (opens SettingsPanel).
    static bool requestPermission(QWidget *parent,
                                  const QString &action,
                                  const QString &filePath);
};
