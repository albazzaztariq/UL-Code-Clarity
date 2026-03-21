#include "core/memoryanalysis.h"
#include "core/theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QPainter>
#include <QPainterPath>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <algorithm>
#include <numeric>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

// ── helpers ──────────────────────────────────────────────────────────────────

static QString fmtBytes(qint64 bytes)
{
    if (bytes < 0)    return "N/A";
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024)
        return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    return QString::number(bytes / (1024.0 * 1024.0), 'f', 2) + " MB";
}

static qint64 queryWorkingSet(QProcess* proc)
{
#ifdef Q_OS_WIN
    if (!proc) return -1;
    DWORD pid = static_cast<DWORD>(proc->processId());
    if (pid == 0) return -1;
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                           FALSE, pid);
    if (!h) return -1;
    PROCESS_MEMORY_COUNTERS pmc = {};
    qint64 ws = -1;
    if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc)))
        ws = static_cast<qint64>(pmc.WorkingSetSize);
    CloseHandle(h);
    return ws;
#else
    Q_UNUSED(proc);
    return -1;
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
MemoryAnalysisFrame::MemoryAnalysisFrame(QWidget* parent)
    : AnalysisFrame("Memory Analysis", parent)
{
    buildUI();

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(100);
    connect(m_pollTimer, &QTimer::timeout, this, &MemoryAnalysisFrame::onPollMemory);
}

void MemoryAnalysisFrame::buildUI()
{
    // ── Content widget added into the inherited results layout ────────────────
    auto* content = new QWidget;
    content->setStyleSheet(QString("QWidget { background: %1; }").arg(Theme::Colors::bg()));
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(32, 24, 32, 32);
    contentLayout->setSpacing(24);

    // ── Section A: Disk Size ──────────────────────────────────────────────
    auto* sectionABox = new QWidget;
    sectionABox->setStyleSheet(QString(
        "QWidget { background: %1; border-radius: 10px; }").arg(Theme::Colors::bg2()));
    auto* sectionALayout = new QVBoxLayout(sectionABox);
    sectionALayout->setContentsMargins(20, 16, 20, 16);
    sectionALayout->setSpacing(10);

    auto* sectionATitle = new QLabel("Disk Size", sectionABox);
    sectionATitle->setStyleSheet(QString(
        "QLabel { color: %1; font-size: 13px; font-weight: bold; background: transparent; }").arg(Theme::Colors::accent()));
    sectionALayout->addWidget(sectionATitle);

    m_diskSourceLabel = new QLabel("Source: —", sectionABox);
    m_diskSourceLabel->setStyleSheet(QString(
        "QLabel { color: %1; font-size: 12px; background: transparent; "
        "font-family: 'Cascadia Code'; }").arg(Theme::Colors::fg()));
    sectionALayout->addWidget(m_diskSourceLabel);

    m_diskBinaryLabel = new QLabel("Binary: —", sectionABox);
    m_diskBinaryLabel->setStyleSheet(QString(
        "QLabel { color: %1; font-size: 12px; background: transparent; "
        "font-family: 'Cascadia Code'; }").arg(Theme::Colors::fg2()));
    sectionALayout->addWidget(m_diskBinaryLabel);

    contentLayout->addWidget(sectionABox);

    // ── Section B: Runtime Memory ─────────────────────────────────────────
    auto* sectionBBox = new QWidget;
    sectionBBox->setStyleSheet(QString(
        "QWidget { background: %1; border-radius: 10px; }").arg(Theme::Colors::bg2()));
    auto* sectionBLayout = new QVBoxLayout(sectionBBox);
    sectionBLayout->setContentsMargins(20, 16, 20, 16);
    sectionBLayout->setSpacing(10);

    auto* sectionBTitle = new QLabel("Runtime Memory", sectionBBox);
    sectionBTitle->setStyleSheet(QString(
        "QLabel { color: %1; font-size: 13px; font-weight: bold; background: transparent; }").arg(Theme::Colors::accent()));
    sectionBLayout->addWidget(sectionBTitle);

    // Stats row
    auto* statsRow = new QHBoxLayout;
    statsRow->setSpacing(32);

    m_currentLabel = new QLabel("Current RAM: —", sectionBBox);
    m_currentLabel->setStyleSheet(QString(
        "QLabel { color: %1; font-size: 12px; background: transparent;"
        " font-family: 'Cascadia Code'; }").arg(Theme::Colors::green()));
    statsRow->addWidget(m_currentLabel);

    m_peakLabel = new QLabel("Peak RAM: —", sectionBBox);
    m_peakLabel->setStyleSheet(
        "QLabel { color: #fab387; font-size: 12px; background: transparent;"
        " font-family: 'Cascadia Code'; }");  // #fab387 = Catppuccin Peach, no Colors method
    statsRow->addWidget(m_peakLabel);

    statsRow->addStretch();
    sectionBLayout->addLayout(statsRow);

    // Chart widget (painted in paintEvent of a sub-widget)
    m_chartWidget = new QWidget(sectionBBox);
    m_chartWidget->setFixedHeight(140);
    m_chartWidget->setStyleSheet(QString("QWidget { background: %1; border-radius: 6px; }").arg(Theme::Colors::bg()));
    m_chartWidget->installEventFilter(this);
    sectionBLayout->addWidget(m_chartWidget);

    m_runtimeStatus = new QLabel("Click \"Run and Monitor\" to start.", sectionBBox);
    m_runtimeStatus->setStyleSheet(QString(
        "QLabel { color: %1; font-size: 11px; background: transparent; }").arg(Theme::Colors::fg3()));
    sectionBLayout->addWidget(m_runtimeStatus);

    m_runBtn = new QPushButton("Run and Monitor", sectionBBox);
    m_runBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border-radius: 6px;"
        " padding: 7px 20px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background: #74c7ec; }"
        "QPushButton:disabled { background: #313244; color: %3; }")
        .arg(Theme::Colors::accent(), Theme::Colors::bg(), Theme::Colors::border()));
    connect(m_runBtn, &QPushButton::clicked, this, &MemoryAnalysisFrame::onRunMonitor);
    sectionBLayout->addWidget(m_runBtn, 0, Qt::AlignLeft);

    m_summaryLabel = new QLabel("", sectionBBox);
    m_summaryLabel->setStyleSheet(QString(
        "QLabel { color: %1; font-size: 12px; background: transparent; }").arg(Theme::Colors::fg()));
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setVisible(false);
    sectionBLayout->addWidget(m_summaryLabel);

    m_noteLabel = new QLabel("", sectionBBox);
    m_noteLabel->setStyleSheet(QString(
        "QLabel { color: %1; font-size: 11px; background: transparent; }").arg(Theme::Colors::yellow()));
    m_noteLabel->setWordWrap(true);
    m_noteLabel->setOpenExternalLinks(true);
    m_noteLabel->setVisible(false);
    sectionBLayout->addWidget(m_noteLabel);

    contentLayout->addWidget(sectionBBox);
    contentLayout->addStretch();

    m_resultsLayout->addWidget(content);
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────
void MemoryAnalysisFrame::setFileInfo(const QString& sourceFilePath,
                                      const QString& binaryFilePath,
                                      const QString& language)
{
    m_sourceFilePath = sourceFilePath;
    m_binaryFilePath = binaryFilePath;
    m_language = language.toLower();

    resetMonitorState();
    updateDiskSection();
}

void MemoryAnalysisFrame::updateDiskSection()
{
    // Source
    if (!m_sourceFilePath.isEmpty()) {
        QFileInfo fi(m_sourceFilePath);
        if (fi.exists()) {
            QString srcStr = QString("Source: %1 (%2)")
                .arg(fi.fileName())
                .arg(fmtBytes(fi.size()));
            m_diskSourceLabel->setText(srcStr);
        } else {
            m_diskSourceLabel->setText("Source: (file not found)");
        }
    } else {
        m_diskSourceLabel->setText("Source: (no file selected)");
    }

    // Binary
    bool interpreted = (m_language == "python" || m_language == "javascript"
                        || m_language == "js");
    if (interpreted) {
        m_diskBinaryLabel->setText(
            QString("Binary: N/A (interpreted — %1)").arg(m_language));
    } else if (!m_binaryFilePath.isEmpty()) {
        QFileInfo bi(m_binaryFilePath);
        if (bi.exists()) {
            m_diskBinaryLabel->setText(
                QString("Binary: %1 (%2)")
                    .arg(bi.fileName())
                    .arg(fmtBytes(bi.size())));
        } else {
            m_diskBinaryLabel->setText("Binary: (not built yet)");
        }
    } else {
        m_diskBinaryLabel->setText("Binary: (not built yet)");
    }
}

void MemoryAnalysisFrame::resetMonitorState()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(500);
    }
    m_pollTimer->stop();
    m_samples.clear();
    m_peakBytes = 0;
    m_running = false;

    m_currentLabel->setText("Current RAM: —");
    m_peakLabel->setText("Peak RAM: —");
    m_runtimeStatus->setText("Click \"Run and Monitor\" to start.");
    m_summaryLabel->setVisible(false);
    m_noteLabel->setVisible(false);
    m_runBtn->setEnabled(true);
    m_runBtn->setText("Run and Monitor");

    if (m_chartWidget) m_chartWidget->update();
}

// ─────────────────────────────────────────────────────────────────────────────
// Slot: Run button
// ─────────────────────────────────────────────────────────────────────────────
void MemoryAnalysisFrame::onRunMonitor()
{
    if (m_running) return;

    // Decide what to execute
    QString executable;
    QStringList args;

    bool interpreted = (m_language == "python" || m_language == "javascript"
                        || m_language == "js");
    if (interpreted) {
        if (m_sourceFilePath.isEmpty()) {
            m_runtimeStatus->setText("No source file selected.");
            return;
        }
        if (m_language == "python") {
            executable = "python";
        } else {
            executable = "node";
        }
        args << m_sourceFilePath;
    } else {
        if (m_binaryFilePath.isEmpty()) {
            m_runtimeStatus->setText("No binary path set. Build the project first.");
            return;
        }
        executable = m_binaryFilePath;
    }

    // Reset display
    m_samples.clear();
    m_peakBytes = 0;
    m_summaryLabel->setVisible(false);
    m_noteLabel->setVisible(false);
    m_runBtn->setEnabled(false);
    m_runBtn->setText("Running...");
    m_runtimeStatus->setText("Monitoring memory usage...");

    // Launch
    if (!m_process) {
        m_process = new QProcess(this);
        connect(m_process,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &MemoryAnalysisFrame::onProcessFinished);
    }
    m_process->start(executable, args);
    if (!m_process->waitForStarted(3000)) {
        m_runtimeStatus->setText(
            QString("Failed to start: %1").arg(m_process->errorString()));
        m_runBtn->setEnabled(true);
        m_runBtn->setText("Run and Monitor");
        return;
    }

    m_running = true;
    m_pollTimer->start();
}

// ─────────────────────────────────────────────────────────────────────────────
// Slot: Poll memory every 100ms
// ─────────────────────────────────────────────────────────────────────────────
void MemoryAnalysisFrame::onPollMemory()
{
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        m_pollTimer->stop();
        return;
    }

    qint64 ws = queryWorkingSet(m_process);
    if (ws >= 0) {
        m_samples.append(ws);
        if (ws > m_peakBytes) m_peakBytes = ws;
        m_currentLabel->setText(QString("Current RAM: %1").arg(fmtBytes(ws)));
        m_peakLabel->setText(QString("Peak RAM: %1").arg(fmtBytes(m_peakBytes)));
    }

    if (m_chartWidget) m_chartWidget->update();
}

// ─────────────────────────────────────────────────────────────────────────────
// Slot: Process finished
// ─────────────────────────────────────────────────────────────────────────────
void MemoryAnalysisFrame::onProcessFinished(int /*exitCode*/, QProcess::ExitStatus /*status*/)
{
    m_pollTimer->stop();
    m_running = false;
    m_runBtn->setEnabled(true);
    m_runBtn->setText("Run and Monitor");

    // Final sample
    qint64 finalWs = queryWorkingSet(m_process);
    if (finalWs >= 0) {
        m_samples.append(finalWs);
        if (finalWs > m_peakBytes) m_peakBytes = finalWs;
    }

    if (m_chartWidget) m_chartWidget->update();

    if (!m_samples.isEmpty()) {
        qint64 sum = std::accumulate(m_samples.begin(), m_samples.end(), (qint64)0);
        qint64 avg = sum / m_samples.count();
        m_runtimeStatus->setText("Program finished.");
        m_summaryLabel->setText(
            QString("Summary — Peak: %1  |  Average: %2  |  Samples: %3")
                .arg(fmtBytes(m_peakBytes))
                .arg(fmtBytes(avg))
                .arg(m_samples.count()));
        m_summaryLabel->setVisible(true);
    } else {
        m_runtimeStatus->setText("Program finished (no memory data collected).");
    }

    // Cython suggestion for Python with heavy usage
    if (m_language == "python" && m_peakBytes > 50LL * 1024 * 1024) {
        m_noteLabel->setText(QString(
            "For better performance, consider Cython for numerical hotspots. "
            "<a href=\"https://cython.org\" style=\"color:%1;\">Learn about Cython →</a>")
            .arg(Theme::Colors::accent()));
        m_noteLabel->setVisible(true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Chart painting — installed event filter draws into m_chartWidget
// ─────────────────────────────────────────────────────────────────────────────
bool MemoryAnalysisFrame::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_chartWidget && event->type() == QEvent::Paint) {
        QPainter painter(m_chartWidget);
        painter.setRenderHint(QPainter::Antialiasing);

        QRect r = m_chartWidget->rect().adjusted(8, 8, -8, -8);

        // Background
        painter.fillRect(m_chartWidget->rect(), Theme::Bg);

        if (m_samples.isEmpty()) {
            painter.setPen(Theme::Fg3);
            painter.drawText(r, Qt::AlignCenter, "No data yet");
            return true;
        }

        qint64 maxSample = *std::max_element(m_samples.begin(), m_samples.end());
        if (maxSample == 0) maxSample = 1;

        int n = m_samples.count();
        QPainterPath path;
        for (int i = 0; i < n; ++i) {
            double x = r.left() + (double)i / (n - 1 < 1 ? 1 : n - 1) * r.width();
            double y = r.bottom() - (double)m_samples[i] / maxSample * r.height();
            if (i == 0)
                path.moveTo(x, y);
            else
                path.lineTo(x, y);
        }

        // Fill under curve
        QPainterPath fill = path;
        fill.lineTo(r.right(), r.bottom());
        fill.lineTo(r.left(), r.bottom());
        fill.closeSubpath();
        QColor fillColor(Theme::Accent.red(), Theme::Accent.green(), Theme::Accent.blue(), 40);
        painter.fillPath(fill, fillColor);

        // Line
        QPen linePen(Theme::Accent, 2);
        painter.setPen(linePen);
        painter.drawPath(path);

        // Y-axis label: max
        painter.setPen(Theme::Fg2);
        painter.setFont(QFont("Segoe UI", 8));
        painter.drawText(r.topLeft() + QPoint(2, 10), fmtBytes(maxSample));

        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void MemoryAnalysisFrame::onHelpClicked()
{
    QMessageBox::information(this, "Memory Analysis Help",
        "Memory Analysis shows you how much memory your program uses.\n\n"
        "Section A — Disk Size:\n"
        "  Shows the size of the source file and (for compiled languages) the binary.\n\n"
        "Section B — Runtime Memory:\n"
        "  Click 'Run and Monitor' to launch your program. Every 100ms, the current\n"
        "  RAM usage (Working Set) is recorded and plotted on the chart.\n"
        "  After the program exits, you'll see peak and average memory usage.\n\n"
        "Working Set = RAM the OS has currently committed to this process.\n"
        "Peak Working Set = the highest value seen during the run.");
}
