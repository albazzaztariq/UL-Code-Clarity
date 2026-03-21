#pragma once

#include "core/analysisframe.h"
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QJsonObject>

// ============================================================================
// ErrorJournal — auto-logging panel for every run/build error
//
// Each entry stores:
//   timestamp, error message, file, line number, fix description
//
// Persisted to <workspaceRoot>/.clarity-errors.json
//
// Layout:
//   [Back to Editor]          Error Journal
//   Most common error: IndexError (hit 5 times)
//   Search: [______________]  [Clear All]
//   ┌────────────────────────────────────────────────────────┐
//   │  2026-03-21  14:32  |  SyntaxError: unexpected indent  │
//   │  file.py line 12                                        │
//   │  Fixed: edited line 14 (next run succeeded)             │
//   └────────────────────────────────────────────────────────┘
//   … more entries …
// ============================================================================

struct ErrorEntry {
    QDateTime timestamp;
    QString   message;        // raw error line
    QString   errorType;      // e.g. "SyntaxError", "IndexError"
    QString   filePath;
    int       lineNumber = 0;
    QString   fixDescription; // filled in on next successful run
    bool      fixed = false;
};

class ErrorJournal : public AnalysisFrame {
    Q_OBJECT

public:
    explicit ErrorJournal(QWidget* parent = nullptr);

    // Set the workspace root — journal file lives at <root>/.clarity-errors.json
    void setWorkspaceRoot(const QString& rootPath);

    // Called by mainwindow when a run/build produces errors
    void logError(const QString& rawOutput, const QString& filePath);

    // Called by mainwindow when a run/build succeeds (marks previous error fixed)
    void logSuccess(const QString& filePath, int linesChanged = 0);

    // Badge count (number of unfixed errors)
    int unfixedCount() const;

signals:
    void jumpToFile(const QString& filePath, int line);
    void errorCountChanged(int count);  // for badge update

private slots:
    void onSearch(const QString& text);
    void onClearAll();
    void onItemClicked(QListWidgetItem* item);

private:
    void loadFromDisk();
    void saveToDisk();
    void rebuildList(const QString& filter = {});
    QString journalPath() const;

    // Parse error type from a raw error message line
    static QString extractErrorType(const QString& msg);
    static int     extractLineNumber(const QString& msg);

    // UI
    QLabel*      m_summaryLabel  = nullptr;
    QLineEdit*   m_searchInput   = nullptr;
    QListWidget* m_list          = nullptr;

    // State
    QString           m_workspaceRoot;
    QList<ErrorEntry> m_entries;
};
