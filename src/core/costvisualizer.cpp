#include "core/costvisualizer.h"
#include "core/theme.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QPainter>
#include <QHBoxLayout>

// ─── Profiler script template ───────────────────────────────────────────────
// Written to a temp file and used as: python <wrapper> <target_file>
// Outputs a single JSON line: {"line_times": {"5": 12345, ...}}
static const char* kProfilerScript = R"py(
import sys, time, json, runpy, os

_times = {}
_last_line = None
_last_t    = None

def _tracer(frame, event, arg):
    global _last_line, _last_t
    if frame.f_code.co_filename != _target:
        return _tracer
    now = time.perf_counter_ns()
    if event == 'line':
        if _last_line is not None and _last_t is not None:
            elapsed = now - _last_t
            _times[_last_line] = _times.get(_last_line, 0) + elapsed
        _last_line = frame.f_lineno
        _last_t    = now
    elif event in ('return', 'exception'):
        if _last_line is not None and _last_t is not None:
            elapsed = now - _last_t
            _times[_last_line] = _times.get(_last_line, 0) + elapsed
        _last_line = None
        _last_t    = None
    return _tracer

if __name__ == '__main__':
    import sys as _sys
    _target = os.path.abspath(_sys.argv[1])
    sys.settrace(_tracer)
    try:
        runpy.run_path(_target, run_name='__main__')
    except SystemExit:
        pass
    except Exception:
        pass
    finally:
        sys.settrace(None)
    print(json.dumps({"line_times": {str(k): v for k, v in _times.items()}}))
)py";

// ─── CostVisualizer ──────────────────────────────────────────────────────────

CostVisualizer::CostVisualizer(QObject* parent)
    : QObject(parent)
{}

CostVisualizer::~CostVisualizer()
{
    clear();
}

void CostVisualizer::clear()
{
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished(500);
        m_process->deleteLater();
        m_process = nullptr;
    }
    m_costs.clear();
    m_rawNs.clear();
    m_totalNs = 0;
    m_active  = false;
}

void CostVisualizer::profileFile(const QString& filePath)
{
    clear();
    m_filePath = filePath;
    m_active   = true;

    emit statusMessage("Profiling " + QFileInfo(filePath).fileName() + "...");

    // Write the profiler wrapper to a temp file
    QString tmpDir    = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString wrapperPath = tmpDir + "/cc_profiler_wrapper.py";
    {
        QFile f(wrapperPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            ts << kProfilerScript;
        }
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &CostVisualizer::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &CostVisualizer::onProcessError);

    QStringList args = {wrapperPath, filePath};
    m_process->start("python", args);
    if (!m_process->waitForStarted(3000)) {
        m_process->start("python3", args);
    }
}

void CostVisualizer::onProcessFinished(int exitCode, QProcess::ExitStatus)
{
    Q_UNUSED(exitCode);
    QByteArray out = m_process->readAllStandardOutput();
    parseOutput(out);

    m_process->deleteLater();
    m_process = nullptr;

    emit statusMessage("Profile complete.");
    emit dataReady();
}

void CostVisualizer::onProcessError(QProcess::ProcessError err)
{
    Q_UNUSED(err);
    m_active = false;
    emit errorOccurred("Failed to launch Python profiler. Is Python installed?");
    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
}

void CostVisualizer::parseOutput(const QByteArray& raw)
{
    // The profiler prints one JSON line at the very end.
    // Find the last line that starts with '{'.
    QByteArray jsonLine;
    for (const QByteArray& line : raw.split('\n')) {
        QByteArray trimmed = line.trimmed();
        if (trimmed.startsWith('{'))
            jsonLine = trimmed;
    }
    if (jsonLine.isEmpty()) return;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonLine, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

    QJsonObject root = doc.object();
    QJsonObject times = root["line_times"].toObject();

    m_rawNs.clear();
    m_totalNs = 0;

    for (auto it = times.begin(); it != times.end(); ++it) {
        int lineNo = it.key().toInt();
        qint64 ns  = static_cast<qint64>(it.value().toDouble());
        m_rawNs[lineNo] = ns;
        m_totalNs += ns;
    }

    // Normalise
    m_costs.clear();
    if (m_totalNs == 0) return;

    qint64 maxNs = *std::max_element(m_rawNs.constBegin(), m_rawNs.constEnd());
    if (maxNs == 0) return;

    for (auto it = m_rawNs.constBegin(); it != m_rawNs.constEnd(); ++it) {
        m_costs[it.key()] = static_cast<double>(it.value()) / static_cast<double>(maxNs);
    }
}

double CostVisualizer::costForLine(int lineNumber) const
{
    auto it = m_costs.constFind(lineNumber);
    if (it == m_costs.constEnd()) return -1.0;
    return it.value();
}

QColor CostVisualizer::colorForCost(double cost)
{
    if (cost < 0.0) {
        // Never executed — grey
        return QColor(0x58, 0x5b, 0x70);
    }
    // Interpolate: blue(0) → yellow(0.5) → red(1.0)
    if (cost <= 0.5) {
        double t = cost * 2.0;  // 0..1
        int r = static_cast<int>(t * 249);
        int g = static_cast<int>(t * 226 + (1.0 - t) * 180);
        int b = static_cast<int>((1.0 - t) * 250);
        return QColor(r, g, b);
    } else {
        double t = (cost - 0.5) * 2.0;  // 0..1
        int r = static_cast<int>(t * 243 + (1.0 - t) * 249);
        int g = static_cast<int>((1.0 - t) * 226);
        int b = static_cast<int>((1.0 - t) * 0);
        return QColor(r, g, b);
    }
}

QString CostVisualizer::tooltipForLine(int lineNumber) const
{
    auto rawIt  = m_rawNs.constFind(lineNumber);
    auto costIt = m_costs.constFind(lineNumber);
    if (rawIt == m_rawNs.constEnd()) return QString();

    qint64 ns = rawIt.value();
    double ms = ns / 1'000'000.0;
    double pct = (m_totalNs > 0)
        ? (static_cast<double>(ns) / static_cast<double>(m_totalNs) * 100.0)
        : 0.0;

    Q_UNUSED(costIt);
    return QString("This line took %1 ms (%2% of total execution time)")
        .arg(ms, 0, 'f', 3)
        .arg(pct, 0, 'f', 1);
}


// ─── CostVisualizerPanel ─────────────────────────────────────────────────────

CostVisualizerPanel::CostVisualizerPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(8, 4, 8, 4);
    lay->setSpacing(8);

    auto* icon = new QLabel("Cost Visualizer");
    icon->setStyleSheet("color: #89b4fa; font-size: 11px; font-weight: bold;");
    lay->addWidget(icon);

    m_statusLabel = new QLabel("Idle");
    m_statusLabel->setStyleSheet("color: #a6adc8; font-size: 11px;");
    lay->addWidget(m_statusLabel, 1);

    m_stopBtn = new QPushButton("Stop");
    m_stopBtn->setFixedHeight(22);
    m_stopBtn->setCursor(Qt::PointingHandCursor);
    m_stopBtn->setStyleSheet(
        "QPushButton { background: #45475a; color: #cdd6f4; border-radius: 4px;"
        " padding: 0 10px; font-size: 11px; }"
        "QPushButton:hover { background: #585b70; }");
    lay->addWidget(m_stopBtn);

    connect(m_stopBtn, &QPushButton::clicked, this, &CostVisualizerPanel::stopRequested);

    setStyleSheet("background: #181825; border-bottom: 1px solid #313244;");
    setFixedHeight(30);
}

void CostVisualizerPanel::setStatus(const QString& text)
{
    if (m_statusLabel) m_statusLabel->setText(text);
}

void CostVisualizerPanel::applyTheme(bool isDark)
{
    m_isDark = isDark;
    if (isDark) {
        setStyleSheet("background: #181825; border-bottom: 1px solid #313244;");
        if (m_statusLabel) m_statusLabel->setStyleSheet("color: #a6adc8; font-size: 11px;");
    } else {
        setStyleSheet("background: #f0f0f0; border-bottom: 1px solid #d0d0d0;");
        if (m_statusLabel) m_statusLabel->setStyleSheet("color: #555555; font-size: 11px;");
    }
}
