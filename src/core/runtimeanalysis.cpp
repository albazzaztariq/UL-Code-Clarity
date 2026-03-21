#include "core/runtimeanalysis.h"
#include "core/optimizer.h"
#include "core/diffview.h"
#include "core/theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QScrollArea>
#include <QStackedWidget>
#include <QPainter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QFont>
#include <algorithm>

// ── Language detection ────────────────────────────────────────────────────
QString RuntimeAnalysisFrame::detectLanguage(const QString& filePath) const
{
    QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == "py")                     return "python";
    if (ext == "c")                      return "c";
    if (ext == "cpp" || ext == "cxx"
        || ext == "cc" || ext == "c++")  return "cpp";
    if (ext == "rs")                     return "rust";
    if (ext == "js")                     return "javascript";
    if (ext == "ul")                     return "ul";
    return "unknown";
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / UI building
// ─────────────────────────────────────────────────────────────────────────────
RuntimeAnalysisFrame::RuntimeAnalysisFrame(QWidget* parent)
    : QWidget(parent)
{
    setAcceptDrops(true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header bar
    auto* headerBar = new QWidget(this);
    headerBar->setFixedHeight(48);
    headerBar->setStyleSheet("background: #2a2a3c; border-bottom: 1px solid #313244;");
    auto* headerLayout = new QHBoxLayout(headerBar);
    headerLayout->setContentsMargins(16, 0, 16, 0);

    auto* title = new QLabel("Runtime Analysis", headerBar);
    title->setStyleSheet(
        "QLabel { color: #cdd6f4; font-size: 15px; font-weight: bold; background: transparent; }");
    headerLayout->addWidget(title);
    headerLayout->addStretch();

    m_backBtn = new QPushButton("Back to Editor", headerBar);
    m_backBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #a6adc8; border-radius: 6px;"
        " padding: 6px 16px; font-size: 12px; }"
        "QPushButton:hover { background: #45475a; color: #cdd6f4; }");
    connect(m_backBtn, &QPushButton::clicked, this, &RuntimeAnalysisFrame::backToEditor);
    headerLayout->addWidget(m_backBtn);
    root->addWidget(headerBar);

    // Stacked panels
    m_stack = new QStackedWidget(this);
    root->addWidget(m_stack, 1);

    m_selectionPanel = new QWidget;
    buildSelectionPanel();
    m_stack->addWidget(m_selectionPanel);

    m_resultsPanel = new QWidget;
    buildResultsPanel();
    m_stack->addWidget(m_resultsPanel);

    // Default: show selection panel
    m_stack->setCurrentWidget(m_selectionPanel);

    connect(this, &RuntimeAnalysisFrame::profileRequested, this,
        [this](const QString& filePath, const QString& language) {
            QString code;
            QFile f(filePath);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text))
                code = QTextStream(&f).readAll();

            CodeOptimizer optimizer;
            QList<OptimizationEntry> entries = optimizer.analyzeFile(code, language, m_assistLevel);

            if (entries.isEmpty()) {
                auto* dlg = new QDialog(this);
                dlg->setWindowTitle("Profile Results");
                dlg->setAttribute(Qt::WA_DeleteOnClose);
                dlg->resize(440, 220);
                auto* lay = new QVBoxLayout(dlg);
                lay->setSpacing(12);
                lay->setContentsMargins(24, 24, 24, 24);
                auto* lbl = new QLabel(
                    "<b>No common issues detected.</b><br><br>"
                    "The patterns we check for weren\xe2\x80\x99t found in this file. "
                    "That\xe2\x80\x99s a good sign \xe2\x80\x94 the code looks clean for the "
                    "beginner pitfalls we look for.", dlg);
                lbl->setWordWrap(true);
                lbl->setTextFormat(Qt::RichText);
                lbl->setStyleSheet("color: #a6e3a1; font-size: 13px; background: transparent;");
                lay->addWidget(lbl);
                auto* btn = new QPushButton("Close", dlg);
                connect(btn, &QPushButton::clicked, dlg, &QDialog::accept);
                lay->addWidget(btn, 0, Qt::AlignCenter);
                dlg->exec();
                return;
            }

            // Show each suggestion in sequence
            for (int idx = 0; idx < entries.size(); ++idx) {
                const OptimizationEntry& entry = entries[idx];

                auto* dlg = new QDialog(this);
                dlg->setWindowTitle(QString("Suggestion %1 of %2 \xe2\x80\x94 %3")
                    .arg(idx + 1).arg(entries.size()).arg(entry.title));
                dlg->setAttribute(Qt::WA_DeleteOnClose);
                dlg->resize(880, 580);

                auto* lay = new QVBoxLayout(dlg);
                lay->setContentsMargins(0, 0, 0, 0);
                lay->setSpacing(0);

                // ── Teaching header ──────────────────────────────────────
                auto* headerWidget = new QWidget(dlg);
                headerWidget->setStyleSheet("background: #2a2a3c; border-bottom: 1px solid #313244;");
                auto* headerLayout = new QVBoxLayout(headerWidget);
                headerLayout->setContentsMargins(16, 14, 16, 14);
                headerLayout->setSpacing(6);

                auto* titleRow = new QHBoxLayout;
                auto* titleLbl = new QLabel(entry.title, headerWidget);
                titleLbl->setStyleSheet(
                    "QLabel { color: #cdd6f4; font-size: 14px; font-weight: bold; background: transparent; }");
                titleRow->addWidget(titleLbl);
                titleRow->addStretch();
                if (entry.lineNumber > 0) {
                    auto* lineLbl = new QLabel(
                        QString("Line %1").arg(entry.lineNumber), headerWidget);
                    lineLbl->setStyleSheet(
                        "QLabel { color: #89b4fa; font-size: 11px; background: #313244;"
                        " border-radius: 3px; padding: 2px 8px; }");
                    titleRow->addWidget(lineLbl);
                }
                headerLayout->addLayout(titleRow);

                // One-sentence summary
                auto* summaryLbl = new QLabel(entry.description, headerWidget);
                summaryLbl->setWordWrap(true);
                summaryLbl->setStyleSheet(
                    "QLabel { color: #a6adc8; font-size: 12px; background: transparent; }");
                headerLayout->addWidget(summaryLbl);

                // Teaching paragraph (why it matters)
                if (!entry.whyItMatters.isEmpty()) {
                    auto* whyLbl = new QLabel(entry.whyItMatters, headerWidget);
                    whyLbl->setWordWrap(true);
                    whyLbl->setStyleSheet(
                        "QLabel { color: #cdd6f4; font-size: 12px; background: transparent;"
                        " padding-top: 6px; }");
                    headerLayout->addWidget(whyLbl);
                }

                lay->addWidget(headerWidget);

                // ── Side-by-side diff ────────────────────────────────────
                auto* diffView = new SideBySideDiffWidget(dlg);
                diffView->setContent(
                    QString("Line %1 \xe2\x80\x94 %2").arg(entry.lineNumber).arg(entry.title),
                    entry.originalCode,
                    entry.suggestedCode);
                lay->addWidget(diffView, 1);

                bool applied = false;
                connect(diffView, &SideBySideDiffWidget::applied, dlg, [&applied, dlg]() {
                    applied = true;
                    dlg->accept();
                });
                connect(diffView, &SideBySideDiffWidget::skipped, dlg, &QDialog::reject);

                dlg->exec();
                Q_UNUSED(applied);
            }
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// Selection panel
// ─────────────────────────────────────────────────────────────────────────────
void RuntimeAnalysisFrame::buildSelectionPanel()
{
    auto* root = new QVBoxLayout(m_selectionPanel);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(16);

    auto* desc = new QLabel(
        "Add two or more files to compare their runtime performance.", m_selectionPanel);
    desc->setStyleSheet("QLabel { color: #a6adc8; font-size: 12px; background: transparent; }");
    root->addWidget(desc);

    // File list (right side in the spec) — we keep it simple: one list + buttons
    auto* listLabel = new QLabel("Files to Compare:", m_selectionPanel);
    listLabel->setStyleSheet("QLabel { color: #cdd6f4; font-size: 12px; font-weight: bold; background: transparent; }");
    root->addWidget(listLabel);

    m_fileList = new QListWidget(m_selectionPanel);
    m_fileList->setStyleSheet(
        "QListWidget { background: #1e1e2e; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; font-size: 12px; }"
        "QListWidget::item { padding: 8px 12px; border-bottom: 1px solid #313244; }"
        "QListWidget::item:selected { background: #313244; }"
        "QListWidget::item:hover { background: #2a2a3c; }");
    m_fileList->setMinimumHeight(200);
    root->addWidget(m_fileList, 1);

    // Buttons row
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    m_browseBtn = new QPushButton("Browse...", m_selectionPanel);
    m_browseBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #cdd6f4; border-radius: 6px;"
        " padding: 7px 18px; font-size: 12px; }"
        "QPushButton:hover { background: #45475a; }");
    connect(m_browseBtn, &QPushButton::clicked, this, &RuntimeAnalysisFrame::onBrowse);
    btnRow->addWidget(m_browseBtn);

    m_removeBtn = new QPushButton("Remove Selected", m_selectionPanel);
    m_removeBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #f38ba8; border-radius: 6px;"
        " padding: 7px 18px; font-size: 12px; }"
        "QPushButton:hover { background: #45475a; }");
    connect(m_removeBtn, &QPushButton::clicked, this, &RuntimeAnalysisFrame::onRemoveFile);
    btnRow->addWidget(m_removeBtn);

    btnRow->addStretch();

    m_runBtn = new QPushButton("Run and Record Runtimes", m_selectionPanel);
    m_runBtn->setEnabled(false);
    m_runBtn->setStyleSheet(
        "QPushButton { background: #89b4fa; color: #1e1e2e; border-radius: 6px;"
        " padding: 8px 24px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: #74c7ec; }"
        "QPushButton:disabled { background: #313244; color: #6c7086; }");
    connect(m_runBtn, &QPushButton::clicked, this, &RuntimeAnalysisFrame::onRunBenchmark);
    btnRow->addWidget(m_runBtn);

    root->addLayout(btnRow);

    // Hint
    auto* hint = new QLabel(
        "Tip: You can also drag and drop files into this window.", m_selectionPanel);
    hint->setStyleSheet("QLabel { color: #6c7086; font-size: 11px; background: transparent; }");
    root->addWidget(hint);
}

// ─────────────────────────────────────────────────────────────────────────────
// Results panel
// ─────────────────────────────────────────────────────────────────────────────
void RuntimeAnalysisFrame::buildResultsPanel()
{
    auto* root = new QVBoxLayout(m_resultsPanel);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(16);

    // Progress area (shown during benchmark, hidden after)
    auto* progressArea = new QWidget(m_resultsPanel);
    auto* progressLayout = new QVBoxLayout(progressArea);
    progressLayout->setContentsMargins(0, 0, 0, 0);
    progressLayout->setSpacing(8);

    m_progressLabel = new QLabel("Running benchmarks...", progressArea);
    m_progressLabel->setStyleSheet(
        "QLabel { color: #cdd6f4; font-size: 13px; background: transparent; }");
    progressLayout->addWidget(m_progressLabel);

    m_progressBar = new QProgressBar(progressArea);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setStyleSheet(
        "QProgressBar { background: #313244; border: none; border-radius: 4px; height: 8px; }"
        "QProgressBar::chunk { background: #89b4fa; border-radius: 4px; }");
    progressLayout->addWidget(m_progressBar);

    root->addWidget(progressArea);

    // Results content area (populated after benchmark)
    m_resultsContent = new QWidget(m_resultsPanel);
    m_resultsContent->setVisible(false);
    root->addWidget(m_resultsContent, 1);

    // "Run Again" button
    auto* runAgainBtn = new QPushButton("Run Again with Different Files", m_resultsPanel);
    runAgainBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #cdd6f4; border-radius: 6px;"
        " padding: 7px 18px; font-size: 12px; }"
        "QPushButton:hover { background: #45475a; }");
    connect(runAgainBtn, &QPushButton::clicked, this, [this]() {
        // Reset and show selection panel
        m_results.clear();
        m_fileList->clear();
        m_runBtn->setEnabled(false);

        if (m_stack) m_stack->setCurrentWidget(m_selectionPanel);
    });
    root->addWidget(runAgainBtn, 0, Qt::AlignLeft);
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────
void RuntimeAnalysisFrame::onBrowse()
{
    QStringList paths = QFileDialog::getOpenFileNames(
        this, "Select Files to Benchmark", QString(),
        "All Files (*);;Python (*.py);;C (*.c);;C++ (*.cpp *.cxx);;Rust (*.rs);;JavaScript (*.js)");

    for (const QString& path : paths)
        addFilePath(path);
}

void RuntimeAnalysisFrame::addFilePath(const QString& path)
{
    if (path.isEmpty()) return;
    QFileInfo fi(path);
    if (!fi.exists()) return;

    // Avoid duplicates
    for (int i = 0; i < m_fileList->count(); ++i) {
        if (m_fileList->item(i)->data(Qt::UserRole).toString() == path)
            return;
    }

    QString lang = detectLanguage(path);
    qint64 size = fi.size();
    QString sizeStr;
    if (size < 1024)
        sizeStr = QString("%1 B").arg(size);
    else if (size < 1024 * 1024)
        sizeStr = QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
    else
        sizeStr = QString("%1 MB").arg(size / (1024.0 * 1024), 0, 'f', 1);

    QString display = QString("%1  [%2]  %3")
        .arg(fi.fileName())
        .arg(lang.toUpper())
        .arg(sizeStr);

    auto* item = new QListWidgetItem(display);
    item->setData(Qt::UserRole, path);
    m_fileList->addItem(item);

    // Enable run button when >= 2 files
    m_runBtn->setEnabled(m_fileList->count() >= 2);
}

void RuntimeAnalysisFrame::onRemoveFile()
{
    auto items = m_fileList->selectedItems();
    for (auto* item : items)
        delete item;
    m_runBtn->setEnabled(m_fileList->count() >= 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Benchmark execution
// ─────────────────────────────────────────────────────────────────────────────
void RuntimeAnalysisFrame::onRunBenchmark()
{
    m_results.clear();
    m_tempExes.clear();

    // Collect files from list
    m_pendingFiles.clear();
    for (int i = 0; i < m_fileList->count(); ++i)
        m_pendingFiles.append(m_fileList->item(i)->data(Qt::UserRole).toString());

    if (m_pendingFiles.size() < 2) return;

    // Switch to results panel
    if (m_stack) m_stack->setCurrentWidget(m_resultsPanel);

    m_progressBar->setValue(0);
    m_progressLabel->setText("Starting benchmarks...");
    m_resultsContent->setVisible(false);

    runNextFile();
}

void RuntimeAnalysisFrame::runNextFile()
{
    if (m_pendingFiles.isEmpty()) {
        finalizeBenchmark();
        return;
    }

    m_currentFile = m_pendingFiles.takeFirst();
    m_currentLang = detectLanguage(m_currentFile);
    m_currentRunIndex = 0;
    m_compiledExe.clear();

    QFileInfo fi(m_currentFile);
    int total = m_results.size() + m_pendingFiles.size() + 1;
    int done  = m_results.size();
    m_progressLabel->setText(QString("Benchmarking: %1 (%2 of %3)...")
        .arg(fi.fileName()).arg(done + 1).arg(total));
    m_progressBar->setValue((done * 100) / total);

    // Need to compile? C, C++, Rust
    if (m_currentLang == "c" || m_currentLang == "cpp" || m_currentLang == "rust") {
        compileFile(m_currentFile, m_currentLang);
    } else {
        // Interpreted — run directly
        runFile(m_currentFile, m_currentLang);
    }
}

void RuntimeAnalysisFrame::compileFile(const QString& filePath, const QString& lang)
{
    m_compiling = true;

    // Build temp output path
    m_compiledExe = QDir::tempPath() + "/ccbench_" +
        QString::number(reinterpret_cast<quintptr>(this), 16) + "_" +
        QFileInfo(filePath).baseName() +
#ifdef Q_OS_WIN
        ".exe";
#else
        "";
#endif
    m_tempExes.append(m_compiledExe);

    QStringList args;
    QString exe;

    if (lang == "c") {
        exe = "gcc";
        args << filePath << "-o" << m_compiledExe << "-O2";
    } else if (lang == "cpp") {
        exe = "g++";
        args << filePath << "-o" << m_compiledExe << "-O2" << "-std=c++17";
    } else if (lang == "rust") {
        exe = "rustc";
        args << filePath << "-o" << m_compiledExe << "-C" << "opt-level=2";
    }

    if (m_process) {
        m_process->kill();
        m_process->deleteLater();
    }
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, &RuntimeAnalysisFrame::onProcessFinished);

    m_process->start(exe, args);
    if (!m_process->waitForStarted(5000)) {
        // Compiler not found — skip this file
        m_compiling = false;
        emit compilationError(filePath,
            QString("Could not start compiler '%1' for file: %2").arg(exe, filePath));
        runNextFile();
    }
}

void RuntimeAnalysisFrame::runFile(const QString& filePath, const QString& lang)
{
    m_compiling = false;

    QString exe;
    QStringList args;

    if (lang == "python") {
        exe = "python";
        args << filePath;
    } else if (lang == "javascript") {
        exe = "node";
        args << filePath;
    } else {
        // Compiled — use the compiled exe
        exe = m_compiledExe;
    }

    if (m_process) {
        m_process->kill();
        m_process->deleteLater();
    }
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, &RuntimeAnalysisFrame::onProcessFinished);

    m_timer.start();
    m_process->start(exe, args);
    if (!m_process->waitForStarted(5000)) {
        // Interpreter not found — record 0 and move on
        recordRunTime(0);
    }
}

void RuntimeAnalysisFrame::onProcessFinished(int exitCode, QProcess::ExitStatus /*status*/)
{
    if (m_compiling) {
        // Compilation done
        if (exitCode != 0) {
            QString errText = m_process ? QString::fromUtf8(m_process->readAll()) : QString();
            emit compilationError(m_currentFile, errText);
            runNextFile();
            return;
        }
        // Start first run
        runFile(m_currentFile, m_currentLang);
        return;
    }

    // A run finished — record time
    qint64 elapsed = m_timer.elapsed();
    recordRunTime(elapsed);
}

void RuntimeAnalysisFrame::recordRunTime(qint64 ms)
{
    if (m_currentRunIndex == 0) {
        // Initialize a new result entry
        BenchmarkResult r;
        r.filePath = m_currentFile;
        r.language = m_currentLang;
        r.runs[0] = ms;
        m_results.append(r);
    } else {
        m_results.last().runs[m_currentRunIndex] = ms;
    }

    ++m_currentRunIndex;

    if (m_currentRunIndex < 3) {
        // Run again
        runFile(m_currentFile, m_currentLang);
    } else {
        // Compute median of 3 runs
        qint64 r0 = m_results.last().runs[0];
        qint64 r1 = m_results.last().runs[1];
        qint64 r2 = m_results.last().runs[2];
        qint64 sorted[3] = {r0, r1, r2};
        std::sort(sorted, sorted + 3);
        m_results.last().medianMs = sorted[1];

        // Move to next file
        runNextFile();
    }
}

void RuntimeAnalysisFrame::finalizeBenchmark()
{
    // Clean up temp exes
    for (const QString& path : m_tempExes)
        QFile::remove(path);
    m_tempExes.clear();

    m_progressBar->setValue(100);
    m_progressLabel->setText("Benchmark complete.");

    // Sort results fastest first
    std::sort(m_results.begin(), m_results.end(),
        [](const BenchmarkResult& a, const BenchmarkResult& b) {
            return a.medianMs < b.medianMs;
        });

    renderResults();
}

// ─────────────────────────────────────────────────────────────────────────────
// Results rendering
// ─────────────────────────────────────────────────────────────────────────────
void RuntimeAnalysisFrame::renderResults()
{
    // Clear old content
    if (m_resultsContent->layout()) {
        QLayout* old = m_resultsContent->layout();
        while (old->count()) {
            QLayoutItem* item = old->takeAt(0);
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        delete old;
    }

    auto* layout = new QVBoxLayout(m_resultsContent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* heading = new QLabel("Results \xe2\x80\x94 fastest to slowest:", m_resultsContent);
    heading->setStyleSheet(
        "QLabel { color: #cdd6f4; font-size: 14px; font-weight: bold; background: transparent; }");
    layout->addWidget(heading);

    // Plain-English summary line (level 1-2 only)
    if (m_assistLevel <= 2 && m_results.size() >= 2) {
        const BenchmarkResult& fastest = m_results.first();
        const BenchmarkResult& slowest = m_results.last();
        QString fastName = QFileInfo(fastest.filePath).fileName();
        QString slowName = QFileInfo(slowest.filePath).fileName();
        QString summaryText;
        if (fastest.medianMs == 0 || slowest.medianMs == 0) {
            summaryText = QString("%1 finished fastest in this test.").arg(fastName);
        } else {
            double ratio = double(slowest.medianMs) / double(fastest.medianMs);
            if (ratio < 1.5) {
                summaryText = QString(
                    "%1 and %2 ran at almost the same speed \xe2\x80\x94 the difference is tiny.")
                    .arg(fastName, slowName);
            } else {
                summaryText = QString(
                    "%1 ran %2x faster than %3. "
                    "Click \"Profile for Improvements\" next to any file to see what could be sped up.")
                    .arg(fastName)
                    .arg(ratio, 0, 'f', 1)
                    .arg(slowName);
            }
        }
        auto* summaryLbl = new QLabel(summaryText, m_resultsContent);
        summaryLbl->setWordWrap(true);
        summaryLbl->setStyleSheet(
            "QLabel { color: #a6adc8; font-size: 12px; background: transparent; padding-bottom: 4px; }");
        layout->addWidget(summaryLbl);
    }

    // Check if Python is in the mix with a compiled language
    bool hasPython = false;
    bool hasCompiled = false;
    for (const BenchmarkResult& r : m_results) {
        if (r.language == "python") hasPython = true;
        if (r.language == "c" || r.language == "cpp" || r.language == "rust") hasCompiled = true;
    }

    qint64 maxMs = m_results.isEmpty() ? 1 : m_results.last().medianMs;
    if (maxMs == 0) maxMs = 1;

    for (int i = 0; i < m_results.size(); ++i) {
        const BenchmarkResult& res = m_results[i];

        // Row widget
        auto* row = new QWidget(m_resultsContent);
        row->setStyleSheet(
            "QWidget { background: #2a2a3c; border-radius: 6px; border: 1px solid #313244; }");
        auto* rowLayout = new QVBoxLayout(row);
        rowLayout->setContentsMargins(14, 10, 14, 10);
        rowLayout->setSpacing(6);

        // Top line: rank, filename, language, time
        auto* topLine = new QHBoxLayout;

        // Rank badge color: green=fastest, red=slowest, yellow=middle
        QString rankColor;
        if (i == 0)
            rankColor = "#a6e3a1";
        else if (i == m_results.size() - 1)
            rankColor = "#f38ba8";
        else
            rankColor = "#f9e2af";

        auto* rankLabel = new QLabel(QString("#%1").arg(i + 1), row);
        rankLabel->setStyleSheet(
            QString("QLabel { color: %1; font-size: 13px; font-weight: bold;"
                    " background: transparent; min-width: 28px; }").arg(rankColor));
        topLine->addWidget(rankLabel);

        auto* nameLabel = new QLabel(QFileInfo(res.filePath).fileName(), row);
        nameLabel->setStyleSheet(
            "QLabel { color: #cdd6f4; font-size: 13px; font-weight: bold; background: transparent; }");
        topLine->addWidget(nameLabel);

        auto* langLabel = new QLabel(res.language.toUpper(), row);
        langLabel->setStyleSheet(
            "QLabel { color: #89b4fa; font-size: 11px; background: #313244;"
            " border-radius: 3px; padding: 2px 6px; }");
        topLine->addWidget(langLabel);

        topLine->addStretch();

        // Format time — plain English at lower assist levels
        QString timeStr;
        if (res.medianMs == 0) {
            timeStr = "< 1 ms";
        } else if (res.medianMs < 1000) {
            timeStr = QString("%1 ms").arg(res.medianMs);
        } else {
            timeStr = QString("%1 s").arg(res.medianMs / 1000.0, 0, 'f', 2);
        }
        // For beginners, add a human-scale hint as a tooltip
        QString timeTooltip;
        if (m_assistLevel <= 2) {
            if (res.medianMs < 10)
                timeTooltip = "Very fast \xe2\x80\x94 finished in under 10 milliseconds";
            else if (res.medianMs < 100)
                timeTooltip = QString("Fast \xe2\x80\x94 about %1 milliseconds "
                    "(a millisecond is 1/1000th of a second)").arg(res.medianMs);
            else if (res.medianMs < 1000)
                timeTooltip = QString("%1 milliseconds \xe2\x80\x94 most programs finish "
                    "in under 1000 ms (1 second)").arg(res.medianMs);
            else
                timeTooltip = QString("%1 seconds \xe2\x80\x94 this is slow for a simple program")
                    .arg(res.medianMs / 1000.0, 0, 'f', 2);
        }
        // Clarify this is the median of 3 runs (invisible detail — no tool names)
        QString timeDetail = QString("Median of 3 runs: %1 ms / %2 ms / %3 ms")
            .arg(res.runs[0]).arg(res.runs[1]).arg(res.runs[2]);

        auto* timeLabel = new QLabel(timeStr, row);
        if (!timeTooltip.isEmpty())
            timeLabel->setToolTip(timeTooltip + "\n\n" + timeDetail);
        else
            timeLabel->setToolTip(timeDetail);
        timeLabel->setStyleSheet(
            QString("QLabel { color: %1; font-size: 14px; font-weight: bold;"
                    " background: transparent; }").arg(rankColor));
        topLine->addWidget(timeLabel);

        rowLayout->addLayout(topLine);

        // Bar chart (custom paint widget)
        double fraction = (maxMs > 0) ? double(res.medianMs) / double(maxMs) : 1.0;
        const QColor barColor = (i == 0)
            ? QColor(0xa6, 0xe3, 0xa1)   // green
            : (i == m_results.size() - 1)
                ? QColor(0xf3, 0x8b, 0xa8) // red
                : QColor(0xf9, 0xe2, 0xaf); // yellow

        // Draw bar inline using a label with fixed height + paintEvent via custom widget
        class BarWidget : public QWidget {
        public:
            double frac;
            QColor color;
            BarWidget(double f, QColor c, QWidget* parent)
                : QWidget(parent), frac(f), color(c) {
                setFixedHeight(12);
                setStyleSheet("background: transparent;");
            }
            void paintEvent(QPaintEvent*) override {
                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing);
                int w = int(width() * frac);
                p.setBrush(color);
                p.setPen(Qt::NoPen);
                p.drawRoundedRect(0, 0, w, height(), 4, 4);
                // Track background
                p.setBrush(QColor(0x31, 0x32, 0x44));
                p.drawRoundedRect(w, 0, width() - w, height(), 4, 4);
            }
        };
        auto* bar = new BarWidget(fraction, barColor, row);
        rowLayout->addWidget(bar);

        // "Profile for Improvements" button
        auto* profileBtn = new QPushButton("Profile for Improvements", row);
        profileBtn->setStyleSheet(
            "QPushButton { background: #313244; color: #89b4fa; border-radius: 5px;"
            " padding: 5px 14px; font-size: 11px; }"
            "QPushButton:hover { background: #45475a; }");
        QString fp = res.filePath;
        QString lang = res.language;
        connect(profileBtn, &QPushButton::clicked, this, [this, fp, lang]() {
            emit profileRequested(fp, lang);
        });
        rowLayout->addWidget(profileBtn, 0, Qt::AlignLeft);

        layout->addWidget(row);
    }

    // Cross-language note if Python vs compiled
    if (hasPython && hasCompiled && m_results.size() >= 2) {
        qint64 fastestMs = m_results.first().medianMs;
        qint64 pythonMs  = 0;
        QString fasterLang;
        for (const BenchmarkResult& r : m_results) {
            if (r.language == "python") { pythonMs = r.medianMs; }
            if (r.language == "c" || r.language == "cpp" || r.language == "rust") {
                if (r.medianMs == fastestMs) fasterLang = r.language;
            }
        }
        if (pythonMs > 0 && fastestMs > 0 && !fasterLang.isEmpty()) {
            double ratio = double(pythonMs) / double(fastestMs);
            auto* noteWidget = new QWidget(m_resultsContent);
            noteWidget->setStyleSheet(
                "QWidget { background: #2a2a1e; border: 1px solid #f9e2af;"
                " border-radius: 6px; }");
            auto* noteLayout = new QVBoxLayout(noteWidget);
            noteLayout->setContentsMargins(14, 10, 14, 10);
            auto* noteLabel = new QLabel(
                CodeOptimizer::crossLanguageNote(fasterLang, ratio, m_assistLevel), noteWidget);
            noteLabel->setWordWrap(true);
            noteLabel->setStyleSheet(
                "QLabel { color: #f9e2af; font-size: 12px; background: transparent; }");
            noteLayout->addWidget(noteLabel);
            layout->addWidget(noteWidget);
        }
    }

    layout->addStretch();

    // Attribution footer — small, unobtrusive, tool names kept internal
    auto* footer = new QLabel(
        "Timing: each file was run 3 times; the middle result (median) is shown to "
        "reduce variance from system load.  \xe2\x80\xa2  "
        "Static analysis powered by Code Clarity's built-in pattern scanner.",
        m_resultsContent);
    footer->setWordWrap(true);
    footer->setStyleSheet(
        "QLabel { color: #45475a; font-size: 10px; background: transparent;"
        " padding: 8px 0 4px 0; }");
    layout->addWidget(footer);

    m_resultsContent->setVisible(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Drag and drop support
// ─────────────────────────────────────────────────────────────────────────────
#include <QDragEnterEvent>
#include <QDropEvent>

void RuntimeAnalysisFrame::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else
        event->ignore();
}

void RuntimeAnalysisFrame::dropEvent(QDropEvent* event)
{
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            addFilePath(url.toLocalFile());
    }
    event->acceptProposedAction();
}
