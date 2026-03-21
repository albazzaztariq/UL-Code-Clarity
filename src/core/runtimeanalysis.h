#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QStringList>
#include <QMap>
#include <QElapsedTimer>
#include <QProcess>

class QDragEnterEvent;
class QDropEvent;

struct BenchmarkResult {
    QString filePath;
    QString language;
    qint64  medianMs = 0;    // median runtime in milliseconds
    qint64  runs[3]  = {};   // individual run times
};

// ============================================================================
// RuntimeAnalysisFrame — full-frame widget replacing the IDE layout
//
// Step 1: File selection panel (left = browser/drop, right = compare list)
// Step 2: Benchmark execution with progress bar
// Step 3: Results ranked list with colored bar chart (QPainter)
// ============================================================================
class RuntimeAnalysisFrame : public QWidget {
    Q_OBJECT

public:
    explicit RuntimeAnalysisFrame(QWidget* parent = nullptr);

signals:
    // Emitted when user wants to go back to the IDE
    void backToEditor();

    // Emitted when compilation of a source file fails;
    // MainWindow should switch back to editor and show the error
    void compilationError(const QString& filePath, const QString& errorText);

    // Emitted when "Profile for Improvements" is clicked for a result
    void profileRequested(const QString& filePath, const QString& language);

private slots:
    void onBrowse();
    void onRemoveFile();
    void onRunBenchmark();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    // ── UI building ──────────────────────────────────────────────────────
    void buildSelectionPanel();
    void buildResultsPanel();
    void showPanel(QWidget* panel);

    // ── File list management ─────────────────────────────────────────────
    void addFilePath(const QString& path);

    // ── Benchmark logic ──────────────────────────────────────────────────
    void runNextFile();
    void compileFile(const QString& filePath, const QString& lang);
    void runFile(const QString& filePath, const QString& lang);
    void recordRunTime(qint64 ms);
    void finalizeBenchmark();
    QString detectLanguage(const QString& filePath) const;

    // ── Result rendering ─────────────────────────────────────────────────
    void renderResults();

    // ── Panels ───────────────────────────────────────────────────────────
    class QStackedWidget* m_stack = nullptr;
    QWidget*     m_selectionPanel = nullptr;
    QWidget*     m_resultsPanel   = nullptr;

    // Selection panel widgets
    QListWidget* m_fileList       = nullptr;   // "Files to Compare" on the right
    QPushButton* m_addBtn         = nullptr;
    QPushButton* m_removeBtn      = nullptr;
    QPushButton* m_browseBtn      = nullptr;
    QPushButton* m_runBtn         = nullptr;   // disabled until 2+ files

    // Results panel widgets
    QWidget*     m_resultsContent = nullptr;   // populated after benchmark
    QProgressBar* m_progressBar   = nullptr;
    QLabel*      m_progressLabel  = nullptr;
    QPushButton* m_backBtn        = nullptr;

    // ── Benchmark state ──────────────────────────────────────────────────
    QStringList              m_pendingFiles;    // files yet to be benchmarked
    int                      m_currentRunIndex = 0;  // 0-2 for three runs
    QString                  m_currentFile;
    QString                  m_currentLang;
    QString                  m_compiledExe;     // temp compiled binary path
    QList<BenchmarkResult>   m_results;
    QElapsedTimer            m_timer;
    QProcess*                m_process         = nullptr;
    bool                     m_compiling       = false;  // true during compile step

    // ── Temp exe tracking ────────────────────────────────────────────────
    QStringList              m_tempExes;   // paths to clean up after benchmark
};
