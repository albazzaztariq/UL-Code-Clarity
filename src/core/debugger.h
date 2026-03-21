#pragma once

#include "core/analysisframe.h"
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QLabel>
#include <QTimer>
#include <QMap>
#include <QSet>
#include <QVector>
#include <QJsonObject>

class DebugBackend;
class DebugEditor;

// ── PossibleCause (inlined from whatswrong.h) ─────────────────────────────
struct PossibleCause {
    QString title;
    QString explanation;
    int     relevantLine = 0;
    QString suggestedFix;
};

// ── WhatsWrongAnalyzer (inlined from whatswrong.h) ────────────────────────
// Pattern-matching analyzer for common runtime errors.
// Error patterns are loaded from data/errors.json at construction time.
class WhatsWrongAnalyzer : public QObject {
    Q_OBJECT
public:
    explicit WhatsWrongAnalyzer(QObject* parent = nullptr);
    QList<PossibleCause> analyzeError(const QString& errorMessage,
                                       const QString& code,
                                       int crashLine) const;
private:
    QVector<QJsonObject> m_errorTypes;
};

// ── ExecutionSnapshot (inlined from executionrecorder.h) ──────────────────
struct ExecutionSnapshot {
    int                     line      = 0;
    QMap<QString, QVariant> variables;
    QStringList             callStack;
};

// ── ExecutionRecorder (inlined from executionrecorder.h) ──────────────────
// Circular buffer of execution snapshots supporting reverse debugging.
class ExecutionRecorder : public QObject {
    Q_OBJECT
public:
    static constexpr int MAX_SNAPSHOTS   = 1000;
    static constexpr int MEMORY_WARN_VARS = 500;

    explicit ExecutionRecorder(QObject* parent = nullptr);

    void record(int line,
                const QMap<QString, QVariant>& variables,
                const QStringList& callStack);
    bool stepBack(ExecutionSnapshot& snap);
    bool stepForward(ExecutionSnapshot& snap);
    int  count()    const { return m_count; }
    int  position() const { return m_position; }
    void reset();

signals:
    void memoryWarning(const QString& message);

private:
    QVector<ExecutionSnapshot> m_buffer;
    int m_head     = 0;
    int m_count    = 0;
    int m_position = 0;
};

// ── Gutter for line numbers + breakpoint dots ────────────────────────────
// Talks directly to DebugEditor which exposes the protected block geometry.
class CodeGutter : public QWidget {
    Q_OBJECT
public:
    explicit CodeGutter(DebugEditor* editor, QWidget* parent = nullptr);

    void setBreakpoints(const QSet<int>& lines);
    void setCurrentLine(int line);
    QSet<int> breakpoints() const { return m_breakpoints; }

signals:
    void breakpointToggled(int line);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    QSize sizeHint() const override;

private:
    DebugEditor* m_editor;
    QSet<int>    m_breakpoints;
    int          m_currentLine = -1;
};

// ── QPlainTextEdit subclass exposing protected block geometry ────────────
class DebugEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit DebugEditor(QWidget* parent = nullptr);

    // Expose the protected members the gutter needs (implemented in .cpp)
    QTextBlock firstBlock()                      const;
    QRectF     blockGeometry(const QTextBlock& b) const;
    QRectF     blockRect(const QTextBlock& b)    const;
    QPointF    contentOff()                      const;
    void       setGutterWidth(int w);
};

// ── Code view (read-only, with gutter) ──────────────────────────────────
class DebugCodeView : public QWidget {
    Q_OBJECT
public:
    explicit DebugCodeView(QWidget* parent = nullptr);

    void setCode(const QString& code);
    void setCurrentLine(int line);   // 1-based
    void setBreakpoints(const QSet<int>& lines);
    QSet<int> breakpoints() const;

    DebugEditor* editor() const { return m_editor; }

signals:
    void breakpointToggled(int line);

private slots:
    void updateGutterWidth();
    void updateGutter(const QRect& rect, int dy);

private:
    DebugEditor* m_editor;
    CodeGutter*  m_gutter;
    int          m_currentLine = -1;
};

// ── Watch panel ──────────────────────────────────────────────────────────
class WatchPanel : public QTreeWidget {
    Q_OBJECT
public:
    explicit WatchPanel(QWidget* parent = nullptr);

    void updateVariables(const QMap<QString, QVariant>& vars);
    void clear();

private:
    void flashItem(QTreeWidgetItem* item);

    QMap<QString, QVariant> m_lastValues;
};

// ── Debug Frame (full replacement view) ─────────────────────────────────
class DebugFrame : public AnalysisFrame {
    Q_OBJECT
public:
    explicit DebugFrame(QWidget* parent = nullptr);

    // Set the file to debug (call before startDebugging)
    void setTargetFile(const QString& filePath, const QString& lang);

    // Start/stop debugging
    void startDebugging();
    void stopDebugging();

    void applyTheme(bool isDark);

private slots:
    void onContinue();
    void onStepOver();
    void onStepInto();
    void onStepOut();
    void onStop();
    void onRestart();
    void onStepBack();

    void onLineChanged(const QString& file, int line);
    void onVariableUpdate(const QMap<QString, QVariant>& vars);
    void onCallStackChanged(const QStringList& frames);
    void onOutput(const QString& text);
    void onCrash(const QString& errorMsg, int crashLine);

private:
    void buildToolbar(QWidget* toolbar);
    void buildWhatsWrongPanel();
    void showWhatsWrongPanel(const QString& error, int line);
    void hideWhatsWrongPanel();

    // UI
    DebugCodeView*  m_codeView;
    WatchPanel*     m_watchPanel;
    QListWidget*    m_callStack;
    QPlainTextEdit* m_console;
    QWidget*        m_whatsWrongPanel;
    QLabel*         m_whatsWrongContent;
    QPushButton*    m_backBtn;

    // Step controls
    QPushButton* m_continueBtn;
    QPushButton* m_stepOverBtn;
    QPushButton* m_stepIntoBtn;
    QPushButton* m_stepOutBtn;
    QPushButton* m_stopBtn;
    QPushButton* m_restartBtn;

    // Status label is inherited from AnalysisFrame (m_statusLabel)

    // Backend + recorder
    DebugBackend*      m_backend;
    ExecutionRecorder* m_recorder;
    WhatsWrongAnalyzer* m_analyzer;

    QString m_targetFile;
    QString m_lang;
    bool    m_isDark = true;
    bool    m_crashed = false;
};
