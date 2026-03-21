#include "core/debugbackend.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QStandardPaths>

DebugBackend::DebugBackend(QObject* parent)
    : QObject(parent)
{
}

DebugBackend::~DebugBackend()
{
    stop();
}

// ── Wrapper script ────────────────────────────────────────────────────────
// Generates a Python wrapper that:
//   1. Sets sys.settrace to intercept every line/call/return/exception
//   2. Sends JSON events on stdout
//   3. Reads commands on stdin (continue / step / into / out / stop)

QString DebugBackend::buildWrapperScript(const QString& scriptPath,
                                          const QSet<int>& breakpoints) const
{
    // Convert breakpoints to a Python set literal
    QStringList bpNums;
    for (int n : breakpoints)
        bpNums << QString::number(n);
    QString bpLiteral = "{" + bpNums.join(", ") + "}";

    // Escape the script path for embedding in Python string
    QString escaped = scriptPath;
    escaped.replace("\\", "\\\\").replace("'", "\\'");

    QString script = R"(
import sys, os, json, threading, types, traceback, io

_TARGET = ')" + escaped + R"('
_BREAKPOINTS = )" + bpLiteral + R"(

# State
_mode = 'run'           # 'run' | 'step' | 'into' | 'out' | 'stop'
_step_depth = 0
_pause_event = threading.Event()
_lock = threading.Lock()

def _emit(obj):
    sys.stdout.write(json.dumps(obj) + '\n')
    sys.stdout.flush()

def _read_commands():
    global _mode
    for line in sys.stdin:
        cmd = line.strip()
        with _lock:
            if cmd == 'stop':
                _mode = 'stop'
                _pause_event.set()
                return
            elif cmd in ('continue', 'step', 'into', 'out'):
                _mode = cmd
                _pause_event.set()

def _vars_to_json(frame):
    result = {}
    try:
        for k, v in frame.f_locals.items():
            if k.startswith('__'):
                continue
            try:
                result[k] = repr(v)
            except Exception:
                result[k] = '<unrepresentable>'
    except Exception:
        pass
    return result

def _stack_frames(frame):
    frames = []
    f = frame
    while f is not None:
        fname = f.f_code.co_name
        ffile = os.path.basename(f.f_code.co_filename)
        fline = f.f_lineno
        frames.append(f'{fname} | {ffile} | {fline}')
        f = f.f_back
    return frames

def _should_pause(frame):
    global _mode
    lineno = frame.f_lineno
    filename = frame.f_code.co_filename
    if _mode == 'stop':
        return True
    if _mode == 'step':
        return True
    if _mode == 'run' and lineno in _BREAKPOINTS:
        return True
    return False

def _tracer(frame, event, arg):
    global _mode, _step_depth
    filename = frame.f_code.co_filename

    # Only trace the target script
    if os.path.abspath(filename) != os.path.abspath(_TARGET):
        return _tracer

    if event == 'line':
        if _should_pause(frame):
            vars_json = _vars_to_json(frame)
            stack = _stack_frames(frame)
            _emit({'event': 'line', 'file': filename,
                   'line': frame.f_lineno,
                   'vars': vars_json,
                   'stack': stack})
            if _mode == 'stop':
                sys.settrace(None)
                return None
            # Wait for a command
            _pause_event.clear()
            _pause_event.wait()
    elif event == 'call':
        pass
    elif event == 'return':
        if _mode == 'out':
            _mode = 'step'
    elif event == 'exception':
        exc_type, exc_val, exc_tb = arg
        _emit({'event': 'crash',
               'msg': f'{exc_type.__name__}: {exc_val}',
               'line': frame.f_lineno})
        sys.settrace(None)
        return None

    return _tracer

# Capture stdout/stderr from the target
class _OutputCapture(io.TextIOBase):
    def write(self, s):
        if s:
            _emit({'event': 'output', 'text': s})
        return len(s)
    def flush(self):
        pass

# Start command reader in background thread
_cmd_thread = threading.Thread(target=_read_commands, daemon=True)
_cmd_thread.start()

# Redirect target's stdout/stderr
_real_stdout = sys.stdout
sys.stdout = _OutputCapture()
sys.stderr = _OutputCapture()

# Install trace and run target
sys.settrace(_tracer)
try:
    with open(_TARGET, 'r') as _f:
        _code = compile(_f.read(), _TARGET, 'exec')
    _globals = {'__name__': '__main__', '__file__': _TARGET}
    exec(_code, _globals)
except SystemExit:
    pass
except Exception as _e:
    _tb = traceback.extract_tb(sys.exc_info()[2])
    _line = _tb[-1].lineno if _tb else 0
    sys.stdout = _real_stdout
    _emit({'event': 'crash', 'msg': f'{type(_e).__name__}: {_e}', 'line': _line})
finally:
    sys.stdout = _real_stdout
    sys.settrace(None)
    _emit({'event': 'done'})
)";

    return script;
}

// ── Public API ────────────────────────────────────────────────────────────

void DebugBackend::startSession(const QString& scriptPath,
                                 const QSet<int>& breakpoints)
{
    stop();  // clean up any prior session

    m_breakpoints = breakpoints;

    // Write wrapper to a temp file
    m_wrapperFile = new QTemporaryFile(
        QDir::tempPath() + "/cc_debug_XXXXXX.py", this);
    m_wrapperFile->setAutoRemove(true);
    if (!m_wrapperFile->open()) {
        emit output("[DebugBackend] Could not create wrapper temp file\n");
        return;
    }
    QString wrapperCode = buildWrapperScript(scriptPath, breakpoints);
    m_wrapperFile->write(wrapperCode.toUtf8());
    m_wrapperFile->flush();
    QString wrapperPath = m_wrapperFile->fileName();
    m_wrapperFile->close();

    // Start the process
    m_process = new QProcess(this);
    m_process->setWorkingDirectory(QFileInfo(scriptPath).absolutePath());
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &DebugBackend::onReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        QString err = QString::fromUtf8(m_process->readAllStandardError());
        if (!err.isEmpty())
            emit output(err);
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &DebugBackend::onProcessFinished);

    m_process->start("python", {wrapperPath});
    if (!m_process->waitForStarted(3000)) {
        m_process->start("python3", {wrapperPath});
        if (!m_process->waitForStarted(3000)) {
            emit output("[DebugBackend] Could not start Python process\n");
            m_process->deleteLater();
            m_process = nullptr;
            return;
        }
    }

    emit output(QString("[Debugger] Started: %1\n").arg(scriptPath));
}

void DebugBackend::setBreakpoints(const QSet<int>& lines)
{
    m_breakpoints = lines;
    // The wrapper reads breakpoints at start; runtime update is sent as JSON command
    if (m_process && m_process->state() == QProcess::Running) {
        QJsonObject cmd;
        cmd["cmd"] = "breakpoints";
        QJsonArray arr;
        for (int n : lines) arr.append(n);
        cmd["lines"] = arr;
        QByteArray data = QJsonDocument(cmd).toJson(QJsonDocument::Compact) + "\n";
        m_process->write(data);
    }
}

void DebugBackend::sendCommand(const QString& cmd)
{
    if (!m_process || m_process->state() != QProcess::Running)
        return;
    m_process->write((cmd + "\n").toUtf8());
}

void DebugBackend::stop()
{
    if (m_process) {
        if (m_process->state() == QProcess::Running) {
            m_process->write("stop\n");
            if (!m_process->waitForFinished(1500))
                m_process->kill();
        }
        m_process->deleteLater();
        m_process = nullptr;
    }
    if (m_wrapperFile) {
        m_wrapperFile->deleteLater();
        m_wrapperFile = nullptr;
    }
    m_buffer.clear();
}

// ── Private ───────────────────────────────────────────────────────────────

void DebugBackend::onReadyRead()
{
    m_buffer += m_process->readAllStandardOutput();

    // Process line by line
    int idx;
    while ((idx = m_buffer.indexOf('\n')) != -1) {
        QByteArray line = m_buffer.left(idx).trimmed();
        m_buffer = m_buffer.mid(idx + 1);
        if (!line.isEmpty())
            parseLine(line);
    }
}

void DebugBackend::parseLine(const QByteArray& line)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        // Not JSON — treat as raw output
        emit output(QString::fromUtf8(line) + "\n");
        return;
    }

    QJsonObject obj = doc.object();
    QString event = obj["event"].toString();

    if (event == "line") {
        QString file = obj["file"].toString();
        int ln = obj["line"].toInt();
        emit lineChanged(file, ln);

        // Variables
        QMap<QString, QVariant> vars;
        QJsonObject varsObj = obj["vars"].toObject();
        for (auto it = varsObj.begin(); it != varsObj.end(); ++it)
            vars[it.key()] = it.value().toVariant();
        emit variableUpdate(vars);

        // Call stack
        QStringList stack;
        QJsonArray stackArr = obj["stack"].toArray();
        for (const auto& frame : stackArr)
            stack.append(frame.toString());
        emit callStackChanged(stack);

    } else if (event == "output") {
        emit output(obj["text"].toString());

    } else if (event == "crash") {
        emit crashed(obj["msg"].toString(), obj["line"].toInt());

    } else if (event == "done") {
        emit finished();
    }
}

void DebugBackend::onProcessFinished(int exitCode, QProcess::ExitStatus /*status*/)
{
    Q_UNUSED(exitCode);
    emit finished();
}
