#include "core/runtimeanalysis.h"
#include "core/optimizer.h"
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
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <algorithm>

// ═══════════════════════════════════════════════════════════════════════
// SideBySideDiffWidget  (merged from diffview.cpp)
// ═══════════════════════════════════════════════════════════════════════

SideBySideDiffWidget::SideBySideDiffWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(700, 400);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet(
        "QLabel { color: #cdd6f4; font-size: 14px; font-weight: bold; "
        "background: transparent; }");
    root->addWidget(m_titleLabel);

    auto* headerRow = new QHBoxLayout;
    auto* leftHeader  = new QLabel("Original", this);
    auto* rightHeader = new QLabel("Suggested", this);
    leftHeader->setStyleSheet(
        "QLabel { color: #f38ba8; font-size: 11px; font-weight: bold; "
        "background: transparent; padding: 2px 4px; }");
    rightHeader->setStyleSheet(
        "QLabel { color: #a6e3a1; font-size: 11px; font-weight: bold; "
        "background: transparent; padding: 2px 4px; }");
    headerRow->addWidget(leftHeader);
    headerRow->addWidget(rightHeader);
    root->addLayout(headerRow);

    auto* panesRow = new QHBoxLayout;
    panesRow->setSpacing(4);

    m_leftPane = new QPlainTextEdit(this);
    m_leftPane->setReadOnly(true);
    m_leftPane->setFont(QFont(Theme::MonoFont, 11));
    m_leftPane->setStyleSheet(
        "QPlainTextEdit { background: #2a1a1e; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; }");

    m_rightPane = new QPlainTextEdit(this);
    m_rightPane->setReadOnly(true);
    m_rightPane->setFont(QFont(Theme::MonoFont, 11));
    m_rightPane->setStyleSheet(
        "QPlainTextEdit { background: #1a2a1e; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; }");

    panesRow->addWidget(m_leftPane);
    panesRow->addWidget(m_rightPane);
    root->addLayout(panesRow, 1);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();

    m_skipBtn = new QPushButton("Skip", this);
    m_skipBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #a6adc8; border-radius: 6px;"
        " padding: 6px 20px; font-size: 12px; }"
        "QPushButton:hover { background: #45475a; }");

    m_applyBtn = new QPushButton("Apply", this);
    m_applyBtn->setStyleSheet(
        "QPushButton { background: #a6e3a1; color: #1e1e2e; border-radius: 6px;"
        " padding: 6px 20px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background: #89d98e; }");

    btnRow->addWidget(m_skipBtn);
    btnRow->addWidget(m_applyBtn);
    root->addLayout(btnRow);

    connect(m_leftPane->verticalScrollBar(),  &QScrollBar::valueChanged,
            this, &SideBySideDiffWidget::syncScrollLeft);
    connect(m_rightPane->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &SideBySideDiffWidget::syncScrollRight);

    connect(m_applyBtn, &QPushButton::clicked, this, &SideBySideDiffWidget::applied);
    connect(m_skipBtn,  &QPushButton::clicked, this, &SideBySideDiffWidget::skipped);
}

void SideBySideDiffWidget::setContent(const QString& title,
                                       const QString& originalCode,
                                       const QString& suggestedCode)
{
    m_titleLabel->setText(title);
    m_leftPane->setPlainText(originalCode);
    m_rightPane->setPlainText(suggestedCode);
    highlightDiffs();
}

void SideBySideDiffWidget::syncScrollLeft(int value)
{
    if (m_syncing) return;
    m_syncing = true;
    m_rightPane->verticalScrollBar()->setValue(value);
    m_syncing = false;
}

void SideBySideDiffWidget::syncScrollRight(int value)
{
    if (m_syncing) return;
    m_syncing = true;
    m_leftPane->verticalScrollBar()->setValue(value);
    m_syncing = false;
}

void SideBySideDiffWidget::highlightDiffs()
{
    QStringList leftLines  = m_leftPane->toPlainText().split('\n');
    QStringList rightLines = m_rightPane->toPlainText().split('\n');

    int maxLines = qMax(leftLines.size(), rightLines.size());

    QList<QTextEdit::ExtraSelection> leftSels, rightSels;

    QColor leftHighlight  = QColor(0xf3, 0x8b, 0xa8, 60);
    QColor rightHighlight = QColor(0xa6, 0xe3, 0xa1, 60);

    auto makeSelection = [](QPlainTextEdit* pane, int lineIdx, const QColor& bg)
        -> QTextEdit::ExtraSelection
    {
        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(bg);
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        QTextCursor cursor(pane->document()->findBlockByLineNumber(lineIdx));
        sel.cursor = cursor;
        return sel;
    };

    for (int i = 0; i < maxLines; ++i) {
        QString l = (i < leftLines.size())  ? leftLines[i]  : QString();
        QString r = (i < rightLines.size()) ? rightLines[i] : QString();
        if (l != r) {
            if (i < leftLines.size())
                leftSels.append(makeSelection(m_leftPane, i, leftHighlight));
            if (i < rightLines.size())
                rightSels.append(makeSelection(m_rightPane, i, rightHighlight));
        }
    }

    m_leftPane->setExtraSelections(leftSels);
    m_rightPane->setExtraSelections(rightSels);
}

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
    : AnalysisFrame("Runtime Analysis", parent)
{
    setAcceptDrops(true);

    // Place the stacked panels into the inherited results scroll area
    m_stack = new QStackedWidget;

    m_selectionPanel = new QWidget;
    buildSelectionPanel();
    m_stack->addWidget(m_selectionPanel);

    m_resultsPanel = new QWidget;
    buildResultsPanel();
    m_stack->addWidget(m_resultsPanel);

    // Default: show selection panel
    m_stack->setCurrentWidget(m_selectionPanel);
    addResultWidget(m_stack);

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
                lbl->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;")
                    .arg(Theme::Colors::green()));
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
                headerWidget->setStyleSheet(QString("background: %1; border-bottom: 1px solid #313244;")
                    .arg(Theme::Colors::bg2()));
                auto* headerLayout = new QVBoxLayout(headerWidget);
                headerLayout->setContentsMargins(16, 14, 16, 14);
                headerLayout->setSpacing(6);

                auto* titleRow = new QHBoxLayout;
                auto* titleLbl = new QLabel(entry.title, headerWidget);
                titleLbl->setStyleSheet(
                    QString("QLabel { color: %1; font-size: 14px; font-weight: bold; background: transparent; }")
                    .arg(Theme::Colors::fg()));
                titleRow->addWidget(titleLbl);
                titleRow->addStretch();
                if (entry.lineNumber > 0) {
                    auto* lineLbl = new QLabel(
                        QString("Line %1").arg(entry.lineNumber), headerWidget);
                    lineLbl->setStyleSheet(
                        QString("QLabel { color: %1; font-size: 11px; background: #313244;"
                        " border-radius: 3px; padding: 2px 8px; }").arg(Theme::Colors::accent()));
                    titleRow->addWidget(lineLbl);
                }
                headerLayout->addLayout(titleRow);

                // One-sentence summary
                auto* summaryLbl = new QLabel(entry.description, headerWidget);
                summaryLbl->setWordWrap(true);
                summaryLbl->setStyleSheet(
                    QString("QLabel { color: %1; font-size: 12px; background: transparent; }")
                    .arg(Theme::Colors::fg2()));
                headerLayout->addWidget(summaryLbl);

                // Teaching paragraph (why it matters)
                if (!entry.whyItMatters.isEmpty()) {
                    auto* whyLbl = new QLabel(entry.whyItMatters, headerWidget);
                    whyLbl->setWordWrap(true);
                    whyLbl->setStyleSheet(
                        QString("QLabel { color: %1; font-size: 12px; background: transparent;"
                        " padding-top: 6px; }").arg(Theme::Colors::fg()));
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
    desc->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; background: transparent; }")
        .arg(Theme::Colors::fg2()));
    root->addWidget(desc);

    // File list (right side in the spec) — we keep it simple: one list + buttons
    auto* listLabel = new QLabel("Files to Compare:", m_selectionPanel);
    listLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; font-weight: bold; background: transparent; }")
        .arg(Theme::Colors::fg()));
    root->addWidget(listLabel);

    m_fileList = new QListWidget(m_selectionPanel);
    m_fileList->setStyleSheet(
        QString("QListWidget { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 4px; font-size: 12px; }"
        "QListWidget::item { padding: 8px 12px; border-bottom: 1px solid #313244; }"
        "QListWidget::item:selected { background: #313244; }"
        "QListWidget::item:hover { background: %4; }")
        .arg(Theme::Colors::bg(), Theme::Colors::fg(),
             Theme::Colors::border(), Theme::Colors::bg2()));
    m_fileList->setMinimumHeight(200);
    root->addWidget(m_fileList, 1);

    // Buttons row
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    m_browseBtn = new QPushButton("Browse...", m_selectionPanel);
    m_browseBtn->setStyleSheet(
        QString("QPushButton { background: #313244; color: %1; border-radius: 6px;"
        " padding: 7px 18px; font-size: 12px; }"
        "QPushButton:hover { background: %2; }").arg(Theme::Colors::fg(), Theme::Colors::border()));
    connect(m_browseBtn, &QPushButton::clicked, this, &RuntimeAnalysisFrame::onBrowse);
    btnRow->addWidget(m_browseBtn);

    m_removeBtn = new QPushButton("Remove Selected", m_selectionPanel);
    m_removeBtn->setStyleSheet(
        QString("QPushButton { background: #313244; color: %1; border-radius: 6px;"
        " padding: 7px 18px; font-size: 12px; }"
        "QPushButton:hover { background: %2; }").arg(Theme::Colors::red(), Theme::Colors::border()));
    connect(m_removeBtn, &QPushButton::clicked, this, &RuntimeAnalysisFrame::onRemoveFile);
    btnRow->addWidget(m_removeBtn);

    btnRow->addStretch();

    m_runBtn = new QPushButton("Run and Record Runtimes", m_selectionPanel);
    m_runBtn->setEnabled(false);
    m_runBtn->setStyleSheet(
        QString("QPushButton { background: %1; color: #1e1e2e; border-radius: 6px;"
        " padding: 8px 24px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: #74c7ec; }"
        "QPushButton:disabled { background: #313244; color: %2; }")
        .arg(Theme::Colors::accent(), Theme::Colors::fg3()));
    connect(m_runBtn, &QPushButton::clicked, this, &RuntimeAnalysisFrame::onRunBenchmark);
    btnRow->addWidget(m_runBtn);

    root->addLayout(btnRow);

    // Hint
    auto* hint = new QLabel(
        "Tip: You can also drag and drop files into this window.", m_selectionPanel);
    hint->setStyleSheet(QString("QLabel { color: %1; font-size: 11px; background: transparent; }")
        .arg(Theme::Colors::fg3()));
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
        QString("QLabel { color: %1; font-size: 13px; background: transparent; }")
        .arg(Theme::Colors::fg()));
    progressLayout->addWidget(m_progressLabel);

    m_progressBar = new QProgressBar(progressArea);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setStyleSheet(
        QString("QProgressBar { background: #313244; border: none; border-radius: 4px; height: 8px; }"
        "QProgressBar::chunk { background: %1; border-radius: 4px; }").arg(Theme::Colors::accent()));
    progressLayout->addWidget(m_progressBar);

    root->addWidget(progressArea);

    // Results content area (populated after benchmark)
    m_resultsContent = new QWidget(m_resultsPanel);
    m_resultsContent->setVisible(false);
    root->addWidget(m_resultsContent, 1);

    // "Run Again" button
    auto* runAgainBtn = new QPushButton("Run Again with Different Files", m_resultsPanel);
    runAgainBtn->setStyleSheet(
        QString("QPushButton { background: #313244; color: %1; border-radius: 6px;"
        " padding: 7px 18px; font-size: 12px; }"
        "QPushButton:hover { background: %2; }").arg(Theme::Colors::fg(), Theme::Colors::border()));
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
        QString("QLabel { color: %1; font-size: 14px; font-weight: bold; background: transparent; }")
        .arg(Theme::Colors::fg()));
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
            QString("QLabel { color: %1; font-size: 12px; background: transparent; padding-bottom: 4px; }")
            .arg(Theme::Colors::fg2()));
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
            QString("QWidget { background: %1; border-radius: 6px; border: 1px solid #313244; }")
            .arg(Theme::Colors::bg2()));
        auto* rowLayout = new QVBoxLayout(row);
        rowLayout->setContentsMargins(14, 10, 14, 10);
        rowLayout->setSpacing(6);

        // Top line: rank, filename, language, time
        auto* topLine = new QHBoxLayout;

        // Rank badge color: green=fastest, red=slowest, yellow=middle
        QString rankColor;
        if (i == 0)
            rankColor = Theme::Colors::green();
        else if (i == m_results.size() - 1)
            rankColor = Theme::Colors::red();
        else
            rankColor = Theme::Colors::yellow();

        auto* rankLabel = new QLabel(QString("#%1").arg(i + 1), row);
        rankLabel->setStyleSheet(
            QString("QLabel { color: %1; font-size: 13px; font-weight: bold;"
                    " background: transparent; min-width: 28px; }").arg(rankColor));
        topLine->addWidget(rankLabel);

        auto* nameLabel = new QLabel(QFileInfo(res.filePath).fileName(), row);
        nameLabel->setStyleSheet(
            QString("QLabel { color: %1; font-size: 13px; font-weight: bold; background: transparent; }")
            .arg(Theme::Colors::fg()));
        topLine->addWidget(nameLabel);

        auto* langLabel = new QLabel(res.language.toUpper(), row);
        langLabel->setStyleSheet(
            QString("QLabel { color: %1; font-size: 11px; background: #313244;"
            " border-radius: 3px; padding: 2px 6px; }").arg(Theme::Colors::accent()));
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
            QString("QPushButton { background: #313244; color: %1; border-radius: 5px;"
            " padding: 5px 14px; font-size: 11px; }"
            "QPushButton:hover { background: %2; }").arg(Theme::Colors::accent(), Theme::Colors::border()));
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
                QString("QWidget { background: #2a2a1e; border: 1px solid %1;"
                " border-radius: 6px; }").arg(Theme::Colors::yellow()));
            auto* noteLayout = new QVBoxLayout(noteWidget);
            noteLayout->setContentsMargins(14, 10, 14, 10);
            auto* noteLabel = new QLabel(
                CodeOptimizer::crossLanguageNote(fasterLang, ratio, m_assistLevel), noteWidget);
            noteLabel->setWordWrap(true);
            noteLabel->setStyleSheet(
                QString("QLabel { color: %1; font-size: 12px; background: transparent; }")
                .arg(Theme::Colors::yellow()));
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
        QString("QLabel { color: %1; font-size: 10px; background: transparent;"
        " padding: 8px 0 4px 0; }").arg(Theme::Colors::border()));
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
