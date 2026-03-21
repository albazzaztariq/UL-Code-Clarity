#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QProcess>
#include <QMap>
#include <QList>

struct DepInfo {
    QString name;
    QString version;     // empty if unknown
    bool    installed = false;
    bool    vulnerable = false;
    QString description; // what this package does
};

// ============================================================================
// DependencyAnalysisFrame — full-frame widget replacing the IDE layout
//
// "Scan Dependencies" button parses current file for imports/includes/requires,
// then checks whether each dependency is installed and what version is present.
//
// Results shown as cards: name | version | status (green=ok, yellow=warn, red=vulnerable)
// Each card has a "What is this?" that shows a description for known packages.
// ============================================================================
class DependencyAnalysisFrame : public QWidget {
    Q_OBJECT

public:
    explicit DependencyAnalysisFrame(QWidget* parent = nullptr);

    // Must be called before showing the frame
    void setCode(const QString& code, const QString& language,
                 const QString& filePath = QString());

signals:
    void backToEditor();

private slots:
    void onScan();
    void onHelpClicked();

private:
    void buildUI();
    void clearResults();
    void showResults(const QList<DepInfo>& deps);
    QWidget* makeCard(const DepInfo& dep);

    // ── Language-specific parsing ─────────────────────────────────────────
    QStringList parsePythonImports(const QString& code) const;
    QStringList parseCIncludes(const QString& code) const;
    QStringList parseJSImports(const QString& code) const;
    QStringList parseRustUses(const QString& code) const;

    // ── Availability checks ───────────────────────────────────────────────
    QList<DepInfo> checkPythonDeps(const QStringList& names);
    QList<DepInfo> checkCDeps(const QStringList& names);
    QList<DepInfo> checkJSDeps(const QStringList& names);
    QList<DepInfo> checkRustDeps(const QStringList& names);

    // ── Known package descriptions (loaded from packages.json) ───────────
    static QMap<QString, QString> knownDescriptions();

    // ── Widgets ───────────────────────────────────────────────────────────
    QPushButton* m_backBtn     = nullptr;
    QPushButton* m_helpBtn     = nullptr;
    QPushButton* m_scanBtn     = nullptr;
    QLabel*      m_statusLabel = nullptr;
    QScrollArea* m_scroll      = nullptr;
    QWidget*     m_cardsWidget = nullptr;

    // ── State ─────────────────────────────────────────────────────────────
    QString m_code;
    QString m_language;
    QString m_filePath;
};
