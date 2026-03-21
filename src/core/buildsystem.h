#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>

// ============================================================================
// BuildSystem — toolchain detection, dependency scanning, run/build execution
//
// Detects installed compilers/runtimes, scans source files for dependencies,
// and executes run/build commands. Output is translated to plain English at
// beginner assist levels (1-2) and shown raw at developer levels (3-4).
// ============================================================================

struct ToolchainInfo {
    bool    found    = false;
    QString path;           // resolved executable path
    QString version;        // e.g. "Python 3.12.1"
};

class BuildSystem : public QObject {
    Q_OBJECT

public:
    explicit BuildSystem(QObject* parent = nullptr);

    // ── Toolchain probing ──────────────────────────────────────────────
    // Run at startup or on demand; results cached.
    void detectToolchains();

    ToolchainInfo pythonInfo()  const { return m_python; }
    ToolchainInfo gccInfo()     const { return m_gcc; }
    ToolchainInfo gppInfo()     const { return m_gpp; }
    ToolchainInfo rustInfo()    const { return m_rust; }
    ToolchainInfo nodeInfo()    const { return m_node; }

    // ── Dependency scanning ────────────────────────────────────────────
    // Returns list of missing pip packages for a Python source file.
    QStringList missingPythonDeps(const QString& sourceCode) const;

    // ── Run / Build ───────────────────────────────────────────────────
    // lang: "python" | "c" | "cpp" | "rust" | "javascript" | "ul"
    // filePath: absolute path to the file to compile/run
    // level: 1-4 assist level (1=beginner friendly output, 4=raw)
    // Emits outputReady() with translated text, then finished().
    void runFile(const QString& filePath, const QString& lang, int level);
    void buildFile(const QString& filePath, const QString& lang, int level);

    // Cancel any running process
    void cancel();

    // True if a build/run process is currently active
    bool isRunning() const;

signals:
    // Plain text (HTML) chunk ready to show in output pane
    void outputReady(const QString& html);
    // Process finished (success = true if exit code 0)
    void finished(bool success);
    // Missing dep detected — ask user to install
    void missingDepsDetected(const QStringList& packages, const QString& installCmd);
    // Toolchain missing — show install link
    void toolchainMissing(const QString& lang, const QString& installUrl);

private slots:
    void onProcessOutput();
    void onProcessError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    // Probe a single command, return version string or empty if not found
    QString probe(const QStringList& candidates, const QStringList& args) const;
    QString findExecutable(const QStringList& candidates) const;

    // Translate raw compiler output to friendly English
    QString translateOutput(const QString& raw, const QString& lang, int level) const;
    QString translateGccError(const QString& line, int level) const;
    QString translatePythonError(const QString& line, int level) const;

    // Emit a styled HTML line to the output pane
    void emitLine(const QString& line, bool isError = false);
    void emitSuccess(const QString& msg);
    void emitInfo(const QString& msg);

    // Build the command + args for run/build
    struct Command {
        QString      exe;
        QStringList  args;
        QString      workingDir;
        bool         valid = false;
        QString      errorMsg;  // set when valid=false
    };
    Command makeRunCommand(const QString& filePath, const QString& lang) const;
    Command makeBuildCommand(const QString& filePath, const QString& lang) const;

    void executeCommand(const Command& cmd, const QString& lang, int level, bool isRun);
    QString languageDisplayName(const QString& lang) const;

    ToolchainInfo m_python;
    ToolchainInfo m_gcc;
    ToolchainInfo m_gpp;
    ToolchainInfo m_rust;
    ToolchainInfo m_node;

    QProcess*  m_process  = nullptr;
    QString    m_rawOutput;
    QString    m_lang;
    int        m_level    = 1;
    bool       m_isRun    = true;

    // Temp output binary (for C/C++/Rust single-file runs)
    mutable QString m_tempExe;
};
