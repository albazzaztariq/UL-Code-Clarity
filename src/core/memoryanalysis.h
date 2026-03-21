#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QProcess>
#include <QVector>
#include <QElapsedTimer>
#include <QEvent>
#include <QPainter>

// ============================================================================
// MemoryAnalysisFrame — full-frame widget replacing the IDE layout
//
// Section A — Disk Size:
//   Shows source file size and (if compiled) binary size on disk.
//
// Section B — Runtime Memory:
//   "Run and Monitor" button launches the program via QProcess.
//   Polls memory every 100ms using Windows PSAPI.
//   Displays current RAM, peak RAM, and a live timeline chart (QPainter).
//   After exit: summary with peak and average.
//   If Python: suggests Cython for heavy workloads.
// ============================================================================
class MemoryAnalysisFrame : public QWidget {
    Q_OBJECT

public:
    explicit MemoryAnalysisFrame(QWidget* parent = nullptr);

    // Must be called before showing the frame
    void setFileInfo(const QString& sourceFilePath,
                     const QString& binaryFilePath,
                     const QString& language);

signals:
    void backToEditor();

private slots:
    void onRunMonitor();
    void onPollMemory();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onHelpClicked();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void buildUI();
    void updateDiskSection();
    void resetMonitorState();

    // ── Header ────────────────────────────────────────────────────────────
    QPushButton* m_backBtn  = nullptr;
    QPushButton* m_helpBtn  = nullptr;

    // ── Section A — Disk ──────────────────────────────────────────────────
    QLabel* m_diskSourceLabel = nullptr;
    QLabel* m_diskBinaryLabel = nullptr;

    // ── Section B — Runtime ───────────────────────────────────────────────
    QPushButton* m_runBtn        = nullptr;
    QLabel*      m_currentLabel  = nullptr;
    QLabel*      m_peakLabel     = nullptr;
    QLabel*      m_statusLabel   = nullptr;
    QLabel*      m_summaryLabel  = nullptr;
    QLabel*      m_noteLabel     = nullptr;   // Cython suggestion for Python
    QWidget*     m_chartWidget   = nullptr;   // chart is painted onto this

    // ── State ─────────────────────────────────────────────────────────────
    QString  m_sourceFilePath;
    QString  m_binaryFilePath;
    QString  m_language;

    QProcess*      m_process   = nullptr;
    QTimer*        m_pollTimer = nullptr;
    QVector<qint64> m_samples;     // working-set samples in bytes
    qint64         m_peakBytes = 0;
    bool           m_running   = false;
};
