#pragma once

#include <QWidget>
#include <QMap>
#include <QProcess>
#include <QString>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>

// ============================================================================
// CostVisualizer — per-line execution cost analysis for Python files.
//
// When toggled on:
//   1. Generates a sys.settrace wrapper script that times each line.
//   2. Runs the target file via QProcess, capturing JSON output:
//      { "line_times": { "5": 123456, "6": 78900, ... } }  (time in ns)
//   3. Normalises values to [0.0, 1.0].
//   4. Signals the CodeEditor to repaint its line number gutter with colours
//      interpolated blue (cheap) → yellow (mid) → red (expensive).
//      Lines never executed are painted grey.
//
// The CodeEditor calls costColor(lineNumber) inside lineNumberAreaPaintEvent.
// ============================================================================

class CostVisualizer : public QObject {
    Q_OBJECT

public:
    explicit CostVisualizer(QObject* parent = nullptr);
    ~CostVisualizer() override;

    // Start profiling the given Python file.  Emits dataReady() when done.
    void profileFile(const QString& filePath);

    // Stop any running process and clear data.
    void clear();

    bool isActive() const { return m_active; }

    // Returns cost in [0,1] for a 1-based line number.
    // Returns -1.0 if the line was never executed or data not yet available.
    double costForLine(int lineNumber) const;

    // Returns a colour for the cost stripe painted next to the line number.
    // cost = -1  → grey (not executed)
    // cost = 0.0 → blue
    // cost = 0.5 → yellow
    // cost = 1.0 → red
    static QColor colorForCost(double cost);

    // Human-readable tooltip for a line.
    QString tooltipForLine(int lineNumber) const;

signals:
    void dataReady();       // profiling complete — editor should repaint
    void errorOccurred(const QString& message);
    void statusMessage(const QString& message);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);

private:
    void parseOutput(const QByteArray& raw);

    QProcess*        m_process  = nullptr;
    QString          m_filePath;
    bool             m_active   = false;

    // line (1-based) → normalised cost [0,1]
    QMap<int, double> m_costs;
    // line (1-based) → raw nanoseconds
    QMap<int, qint64> m_rawNs;
    qint64            m_totalNs  = 0;
};


// ============================================================================
// CostVisualizerPanel — thin UI panel shown when cost visualiser is active.
// Floats as a header bar above the editor, showing status + a Stop button.
// ============================================================================
class CostVisualizerPanel : public QWidget {
    Q_OBJECT

public:
    explicit CostVisualizerPanel(QWidget* parent = nullptr);

    void setStatus(const QString& text);
    void applyTheme(bool isDark);

signals:
    void stopRequested();

private:
    QLabel*      m_statusLabel = nullptr;
    QPushButton* m_stopBtn     = nullptr;
    bool         m_isDark      = true;
};
