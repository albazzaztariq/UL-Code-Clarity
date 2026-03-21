#include "core/buildsystem.h"

#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
// Helper to hide console windows from QProcess on Windows
static void hideConsoleWindow(QProcess &p) {
    p.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NO_WINDOW;
    });
}
#else
static void hideConsoleWindow(QProcess &) {}
#endif

// ============================================================================
// BuildSystem
// ============================================================================

BuildSystem::BuildSystem(QObject* parent)
    : QObject(parent)
{
    detectToolchains();
}

// ── Toolchain detection ──────────────────────────────────────────────────────

void BuildSystem::detectToolchains()
{
    // Python — try py (Windows launcher), python3, python
    m_python.version = probe({"py", "python3", "python"}, {"--version"});
    m_python.found   = !m_python.version.isEmpty();
    m_python.path    = findExecutable({"py", "python3", "python"});

    // GCC
    m_gcc.version = probe({"gcc"}, {"--version"});
    m_gcc.found   = !m_gcc.version.isEmpty();
    m_gcc.path    = findExecutable({"gcc"});

    // G++
    m_gpp.version = probe({"g++"}, {"--version"});
    m_gpp.found   = !m_gpp.version.isEmpty();
    m_gpp.path    = findExecutable({"g++"});

    // Rust — cargo is the preferred entry point
    m_rust.version = probe({"rustc"}, {"--version"});
    m_rust.found   = !m_rust.version.isEmpty();
    m_rust.path    = findExecutable({"rustc"});

    // Node.js
    m_node.version = probe({"node", "nodejs"}, {"--version"});
    m_node.found   = !m_node.version.isEmpty();
    m_node.path    = findExecutable({"node", "nodejs"});
}

QString BuildSystem::probe(const QStringList& candidates, const QStringList& args) const
{
    for (const QString& exe : candidates) {
        QProcess p;
        hideConsoleWindow(p);
        p.start(exe, args);
        if (p.waitForFinished(3000)) {
            QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
            if (out.isEmpty())
                out = QString::fromUtf8(p.readAllStandardError()).trimmed();
            // Keep just the first line
            out = out.split('\n').first().trimmed();
            if (!out.isEmpty())
                return out;
        }
    }
    return {};
}

QString BuildSystem::findExecutable(const QStringList& candidates) const
{
    for (const QString& exe : candidates) {
        QString found = QStandardPaths::findExecutable(exe);
        if (!found.isEmpty())
            return found;
    }
    return {};
}

// ── Dependency scanning ──────────────────────────────────────────────────────

QStringList BuildSystem::missingPythonDeps(const QString& sourceCode) const
{
    // Collect all import statements
    static const QRegularExpression importRe(
        R"(^\s*(?:import|from)\s+([\w\.]+))",
        QRegularExpression::MultilineOption);

    // Standard library modules that are always available — don't pip-check these
    static const QSet<QString> stdlib = {
        "os","sys","re","math","json","time","datetime","pathlib","typing",
        "collections","itertools","functools","io","abc","copy","enum","random",
        "string","struct","threading","subprocess","socket","logging","hashlib",
        "base64","urllib","http","html","xml","csv","sqlite3","tempfile",
        "shutil","glob","fnmatch","stat","platform","ctypes","gc","weakref",
        "inspect","traceback","warnings","argparse","contextlib","dataclasses",
        "pprint","textwrap","unicodedata","codecs","queue","heapq","bisect",
        "decimal","fractions","statistics","cmath","operator","array","mmap",
        "signal","errno","builtins","ast","dis","tokenize","token","keyword",
        "symtable","compileall","py_compile","zipfile","tarfile","gzip","bz2",
        "lzma","zlib","uuid","secrets","hmac","ssl","email","mailbox","smtplib",
        "imaplib","ftplib","telnetlib","xmlrpc","wsgiref","cgi","cgitb",
        "unittest","doctest","pdb","profile","cProfile","timeit","trace",
        "tkinter","turtle","curses","readline","rlcompleter","getpass","getopt",
        "__future__","_thread","multiprocessing","concurrent","asyncio",
        "selectors","sched","calendar","locale","gettext","cmd","shlex",
        "configparser","tomllib","pickle","shelve","dbm","venv","zipimport",
        "importlib","pkgutil","pkg_resources","site","code","codeop","pyclbr",
        "fileinput","filecmp","linecache","difflib","enum","types","typing_extensions"
    };

    QSet<QString> imports;
    auto it = importRe.globalMatch(sourceCode);
    while (it.hasNext()) {
        auto m = it.next();
        QString top = m.captured(1).split('.').first();
        if (!stdlib.contains(top) && !top.isEmpty())
            imports.insert(top);
    }

    if (imports.isEmpty())
        return {};

    QStringList missing;
    for (const QString& pkg : imports) {
        QProcess p;
        hideConsoleWindow(p);
        QStringList pyArgs = {"py", "python3", "python"};
        for (const QString& py : pyArgs) {
            p.start(py, {"-m", "pip", "show", pkg});
            if (p.waitForFinished(5000)) {
                if (p.exitCode() != 0)
                    missing.append(pkg);
                break;
            }
        }
    }
    return missing;
}

// ── Command construction ─────────────────────────────────────────────────────

BuildSystem::Command BuildSystem::makeRunCommand(const QString& filePath,
                                                  const QString& lang) const
{
    Command cmd;
    QFileInfo fi(filePath);
    cmd.workingDir = fi.absolutePath();

    if (lang == "python") {
        if (!m_python.found) {
            cmd.errorMsg = "Python is not installed on this system.";
            return cmd;
        }
        cmd.exe  = m_python.path.isEmpty() ? "python" : m_python.path;
        cmd.args = {filePath};
        cmd.valid = true;

    } else if (lang == "c") {
        if (!m_gcc.found) {
            cmd.errorMsg = "GCC is not installed. Install MinGW-w64 to compile C code.";
            return cmd;
        }
        // Compile then run: gcc file.c -o temp && temp
        // We return a two-stage command; executeCommand handles this
        m_tempExe = QDir::toNativeSeparators(
            cmd.workingDir + "/cc_run_" + fi.baseName() + ".exe");
        cmd.exe  = m_gcc.path.isEmpty() ? "gcc" : m_gcc.path;
        cmd.args = {filePath, "-o", m_tempExe, "-lm"};
        cmd.valid = true;

    } else if (lang == "cpp") {
        if (!m_gpp.found) {
            cmd.errorMsg = "G++ is not installed. Install MinGW-w64 to compile C++ code.";
            return cmd;
        }
        m_tempExe = QDir::toNativeSeparators(
            cmd.workingDir + "/cc_run_" + fi.baseName() + ".exe");
        cmd.exe  = m_gpp.path.isEmpty() ? "g++" : m_gpp.path;
        cmd.args = {filePath, "-o", m_tempExe, "-std=c++17"};
        cmd.valid = true;

    } else if (lang == "rust") {
        if (!m_rust.found) {
            cmd.errorMsg = "Rust is not installed. Visit rustup.rs to install.";
            return cmd;
        }
        m_tempExe = QDir::toNativeSeparators(
            cmd.workingDir + "/cc_run_" + fi.baseName() + ".exe");
        cmd.exe  = m_rust.path.isEmpty() ? "rustc" : m_rust.path;
        cmd.args = {filePath, "-o", m_tempExe};
        cmd.valid = true;

    } else if (lang == "javascript") {
        if (!m_node.found) {
            cmd.errorMsg = "Node.js is not installed. Visit nodejs.org to install.";
            return cmd;
        }
        cmd.exe  = m_node.path.isEmpty() ? "node" : m_node.path;
        cmd.args = {filePath};
        cmd.valid = true;

    } else if (lang == "ul") {
        // UniLogic: transpile to C then compile and run
        if (!m_python.found) {
            cmd.errorMsg = "Python is required to run UniLogic programs.";
            return cmd;
        }
        if (!m_gcc.found) {
            cmd.errorMsg = "GCC is required to compile UniLogic programs.";
            return cmd;
        }
        // Look for XPile Main.py relative to executable
        QString xpile = QCoreApplication::applicationDirPath() + "/XPile/Main.py";
        if (!QFileInfo::exists(xpile))
            xpile = QCoreApplication::applicationDirPath() + "/Main.py";
        if (!QFileInfo::exists(xpile)) {
            cmd.errorMsg = "UniLogic compiler (XPile) not found next to the application.";
            return cmd;
        }
        cmd.exe  = m_python.path.isEmpty() ? "python" : m_python.path;
        cmd.args = {xpile, filePath, "--target", "c"};
        cmd.valid = true;

    } else {
        cmd.errorMsg = QString("Don't know how to run '%1' files.").arg(lang);
    }
    return cmd;
}

BuildSystem::Command BuildSystem::makeBuildCommand(const QString& filePath,
                                                    const QString& lang) const
{
    // Build = compile only (no run).  For Python/JS, "build" = syntax check.
    Command cmd;
    QFileInfo fi(filePath);
    cmd.workingDir = fi.absolutePath();

    if (lang == "python") {
        if (!m_python.found) {
            cmd.errorMsg = "Python is not installed on this system.";
            return cmd;
        }
        cmd.exe   = m_python.path.isEmpty() ? "python" : m_python.path;
        cmd.args  = {"-m", "py_compile", filePath};
        cmd.valid = true;

    } else if (lang == "c") {
        if (!m_gcc.found) {
            cmd.errorMsg = "GCC is not installed. Install MinGW-w64 to compile C code.";
            return cmd;
        }
        m_tempExe = QDir::toNativeSeparators(
            cmd.workingDir + "/cc_build_" + fi.baseName() + ".exe");
        cmd.exe   = m_gcc.path.isEmpty() ? "gcc" : m_gcc.path;
        cmd.args  = {filePath, "-o", m_tempExe, "-lm"};
        cmd.valid = true;

    } else if (lang == "cpp") {
        if (!m_gpp.found) {
            cmd.errorMsg = "G++ is not installed. Install MinGW-w64 to compile C++ code.";
            return cmd;
        }
        m_tempExe = QDir::toNativeSeparators(
            cmd.workingDir + "/cc_build_" + fi.baseName() + ".exe");
        cmd.exe   = m_gpp.path.isEmpty() ? "g++" : m_gpp.path;
        cmd.args  = {filePath, "-o", m_tempExe, "-std=c++17"};
        cmd.valid = true;

    } else if (lang == "rust") {
        if (!m_rust.found) {
            cmd.errorMsg = "Rust is not installed. Visit rustup.rs to install.";
            return cmd;
        }
        m_tempExe = QDir::toNativeSeparators(
            cmd.workingDir + "/cc_build_" + fi.baseName() + ".exe");
        cmd.exe   = m_rust.path.isEmpty() ? "rustc" : m_rust.path;
        cmd.args  = {filePath, "-o", m_tempExe};
        cmd.valid = true;

    } else if (lang == "javascript") {
        if (!m_node.found) {
            cmd.errorMsg = "Node.js is not installed. Visit nodejs.org to install.";
            return cmd;
        }
        // Syntax check only
        cmd.exe   = m_node.path.isEmpty() ? "node" : m_node.path;
        cmd.args  = {"--check", filePath};
        cmd.valid = true;

    } else {
        cmd.errorMsg = QString("Don't know how to build '%1' files.").arg(lang);
    }
    return cmd;
}

// ── Public run/build entry points ────────────────────────────────────────────

void BuildSystem::runFile(const QString& filePath, const QString& lang, int level)
{
    if (isRunning()) {
        emitLine("A process is already running. Cancel it first.", true);
        return;
    }

    m_lang  = lang;
    m_level = level;
    m_isRun = true;
    m_tempExe.clear();
    m_rawOutput.clear();

    // Check for missing Python deps before running
    if (lang == "python" && m_python.found) {
        QFile f(filePath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString src = QString::fromUtf8(f.readAll());
            f.close();
            QStringList missing = missingPythonDeps(src);
            if (!missing.isEmpty()) {
                QString installCmd = QString("pip install %1").arg(missing.join(' '));
                emit missingDepsDetected(missing, installCmd);
                return;
            }
        }
    }

    Command cmd = makeRunCommand(filePath, lang);
    if (!cmd.valid) {
        // Toolchain missing
        QString url;
        if (lang == "python")     url = "https://www.python.org/downloads/";
        else if (lang == "c")     url = "https://www.mingw-w64.org/downloads/";
        else if (lang == "cpp")   url = "https://www.mingw-w64.org/downloads/";
        else if (lang == "rust")  url = "https://rustup.rs/";
        else if (lang == "javascript") url = "https://nodejs.org/";
        emit toolchainMissing(lang, url);
        emitLine(cmd.errorMsg, true);
        emit finished(false);
        return;
    }

    if (level <= 2)
        emitInfo(QString("Running your %1 program...").arg(languageDisplayName(lang)));
    else
        emitInfo(QString("$ %1 %2").arg(cmd.exe, cmd.args.join(' ')));

    executeCommand(cmd, lang, level, true);
}

void BuildSystem::buildFile(const QString& filePath, const QString& lang, int level)
{
    if (isRunning()) {
        emitLine("A process is already running. Cancel it first.", true);
        return;
    }

    m_lang  = lang;
    m_level = level;
    m_isRun = false;
    m_tempExe.clear();
    m_rawOutput.clear();

    Command cmd = makeBuildCommand(filePath, lang);
    if (!cmd.valid) {
        QString url;
        if (lang == "python")     url = "https://www.python.org/downloads/";
        else if (lang == "c")     url = "https://www.mingw-w64.org/downloads/";
        else if (lang == "cpp")   url = "https://www.mingw-w64.org/downloads/";
        else if (lang == "rust")  url = "https://rustup.rs/";
        else if (lang == "javascript") url = "https://nodejs.org/";
        emit toolchainMissing(lang, url);
        emitLine(cmd.errorMsg, true);
        emit finished(false);
        return;
    }

    if (level <= 2)
        emitInfo(QString("Building your %1 program...").arg(languageDisplayName(lang)));
    else
        emitInfo(QString("$ %1 %2").arg(cmd.exe, cmd.args.join(' ')));

    executeCommand(cmd, lang, level, false);
}

// ── Process execution ─────────────────────────────────────────────────────────

void BuildSystem::executeCommand(const Command& cmd, const QString& lang,
                                  int level, bool isRun)
{
    m_process = new QProcess(this);
    hideConsoleWindow(*m_process);
    m_process->setWorkingDirectory(cmd.workingDir);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyRead,        this, &BuildSystem::onProcessOutput);
    connect(m_process, &QProcess::errorOccurred,    this, &BuildSystem::onProcessError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &BuildSystem::onProcessFinished);

    m_process->start(cmd.exe, cmd.args);
    if (!m_process->waitForStarted(5000)) {
        emitLine(QString("Could not start: %1").arg(cmd.exe), true);
        m_process->deleteLater();
        m_process = nullptr;
        emit finished(false);
    }
}

void BuildSystem::onProcessOutput()
{
    if (!m_process) return;
    QString chunk = QString::fromUtf8(m_process->readAll());
    m_rawOutput += chunk;

    // Show each line as it arrives
    const QStringList lines = chunk.split('\n');
    for (const QString& line : lines) {
        if (line.trimmed().isEmpty()) continue;
        if (m_level <= 2) {
            // Translate to friendly English
            QString friendly;
            if (m_lang == "python")
                friendly = translatePythonError(line, m_level);
            else if (m_lang == "c" || m_lang == "cpp")
                friendly = translateGccError(line, m_level);
            else
                friendly = line;
            if (!friendly.isEmpty())
                emitLine(friendly, line.contains("error:") || line.contains("Error"));
        } else {
            emitLine(line, line.contains("error:") || line.contains("Error"));
        }
    }
}

void BuildSystem::onProcessError()
{
    // QProcess::errorOccurred — only fires when the exe can't be started
    if (m_process)
        emitLine("Could not start the process: " + m_process->errorString(), true);
}

void BuildSystem::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status);
    bool success = (exitCode == 0);

    // For compiled languages in run-mode: if compile succeeded, now run the exe
    bool needRunExe = m_isRun && !m_tempExe.isEmpty() && success
                      && (m_lang == "c" || m_lang == "cpp" || m_lang == "rust");

    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }

    if (needRunExe) {
        // Compile succeeded — now run the produced binary
        if (m_level <= 2)
            emitInfo("Compiled successfully. Running your program...");
        else
            emitInfo(QString("$ %1").arg(m_tempExe));

        Command runCmd;
        runCmd.exe        = m_tempExe;
        runCmd.workingDir = QFileInfo(m_tempExe).absolutePath();
        runCmd.valid      = true;
        m_tempExe.clear();  // don't re-enter this branch
        executeCommand(runCmd, m_lang, m_level, false);
        return;
    }

    // Final completion
    if (success) {
        if (m_isRun) {
            if (m_level <= 2)
                emitSuccess("Program finished.");
            else
                emitSuccess(QString("Process exited with code %1.").arg(exitCode));
        } else {
            // Build-only
            if (m_lang == "python" || m_lang == "javascript") {
                if (m_level <= 2)
                    emitSuccess("No errors found — your code looks good!");
                else
                    emitSuccess("Syntax check passed.");
            } else {
                if (m_level <= 2)
                    emitSuccess("Build succeeded — 0 errors.");
                else
                    emitSuccess(QString("Build succeeded. Exit code: %1.").arg(exitCode));
            }
        }
    } else {
        if (m_level <= 2) {
            emitLine("Build failed. Check the errors above — each one tells you what to fix.", true);
        } else {
            emitLine(QString("Process exited with code %1.").arg(exitCode), true);
        }
    }

    // Clean up temp exe on failure
    if (!success && !m_tempExe.isEmpty()) {
        QFile::remove(m_tempExe);
        m_tempExe.clear();
    }

    emit finished(success);
}

// ── Translation helpers ───────────────────────────────────────────────────────

QString BuildSystem::translateGccError(const QString& line, int level) const
{
    Q_UNUSED(level);
    // Pattern: file:line:col: error: message
    static const QRegularExpression errRe(
        R"(([^:]+):(\d+):(\d+):\s*(error|warning|note):\s*(.+))");
    auto m = errRe.match(line);
    if (!m.hasMatch())
        return line;

    QString file    = QFileInfo(m.captured(1)).fileName();
    QString lineNo  = m.captured(2);
    QString kind    = m.captured(4);
    QString msg     = m.captured(5).trimmed();

    // Translate common messages
    if (msg.contains("undeclared") || msg.contains("was not declared"))
        msg = QString("'%1' doesn't exist yet — did you forget to create or import it?")
              .arg(msg.section("'", 1, 1));
    else if (msg.contains("expected ';'"))
        msg = "Missing semicolon — add a ; at the end of the previous line.";
    else if (msg.contains("expected '}'") || msg.contains("expected '{'"))
        msg = "Mismatched braces — check that every { has a matching }.";
    else if (msg.contains("conflicting types"))
        msg = "You declared this variable or function with two different types.";
    else if (msg.contains("implicit declaration"))
        msg = "This function is used before it's declared. Move the definition up or add a prototype.";
    else if (msg.contains("unused variable"))
        msg = "You created this variable but never used it. You can remove it.";
    else if (msg.contains("control reaches end of non-void function"))
        msg = "This function is supposed to return a value, but not all paths return one.";

    if (kind == "error")
        return QString("Line %1: Error — %2").arg(lineNo, msg);
    else if (kind == "warning")
        return QString("Line %1: Warning — %2").arg(lineNo, msg);
    return QString("Line %1: %2 — %3").arg(lineNo, kind, msg);
}

QString BuildSystem::translatePythonError(const QString& line, int level) const
{
    Q_UNUSED(level);
    // Python tracebacks: pass through with light decoration
    if (line.startsWith("Traceback"))
        return "Your program crashed:";
    if (line.trimmed().startsWith("File \"") && line.contains(", line "))
        return "  " + line.trimmed();
    if (line.contains("NameError"))
        return "  Name not found — you used a name that hasn't been defined yet.";
    if (line.contains("SyntaxError"))
        return "  Syntax error — Python couldn't understand this line.";
    if (line.contains("IndentationError"))
        return "  Indentation error — check that your spaces/tabs are consistent.";
    if (line.contains("ModuleNotFoundError") || line.contains("ImportError"))
        return "  Missing module — a package this file needs isn't installed.";
    if (line.contains("TypeError"))
        return "  Type error — you used a value in a way that doesn't match its type.";
    if (line.contains("ValueError"))
        return "  Value error — a value was the right type but had an unexpected value.";
    if (line.contains("AttributeError"))
        return "  Attribute error — you tried to use something the object doesn't have.";
    if (line.contains("ZeroDivisionError"))
        return "  Division by zero — you divided by 0, which isn't allowed.";
    if (line.contains("IndexError"))
        return "  Index out of range — you tried to access a list item that doesn't exist.";
    if (line.contains("KeyError"))
        return "  Key not found — you looked up a dictionary key that isn't there.";
    return line;
}

// ── Output emitters ───────────────────────────────────────────────────────────

void BuildSystem::emitLine(const QString& line, bool isError)
{
    QString html;
    if (isError)
        html = QString("<span style='color:#cc3333;'>%1</span>").arg(line.toHtmlEscaped());
    else
        html = line.toHtmlEscaped();
    emit outputReady(html + "\n");
}

void BuildSystem::emitSuccess(const QString& msg)
{
    emit outputReady(
        QString("<span style='color:#1a7a1a; font-weight:600;'>&#10003; %1</span>\n")
        .arg(msg.toHtmlEscaped()));
}

void BuildSystem::emitInfo(const QString& msg)
{
    emit outputReady(
        QString("<span style='color:#555555;'>%1</span>\n")
        .arg(msg.toHtmlEscaped()));
}

// ── Cancel ────────────────────────────────────────────────────────────────────

void BuildSystem::cancel()
{
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished(2000);
        m_process->deleteLater();
        m_process = nullptr;
        emitLine("Process cancelled.", false);
        emit finished(false);
    }
}

bool BuildSystem::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString BuildSystem::languageDisplayName(const QString& lang) const
{
    if (lang == "python")     return "Python";
    if (lang == "c")          return "C";
    if (lang == "cpp")        return "C++";
    if (lang == "rust")       return "Rust";
    if (lang == "javascript") return "JavaScript";
    if (lang == "ul")         return "UniLogic";
    return lang;
}
