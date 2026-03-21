#include "core/errorjournal.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QListWidgetItem>
#include <QRegularExpression>
#include <QMap>
#include <QFont>

// ============================================================================
// ErrorJournal
// ============================================================================

ErrorJournal::ErrorJournal(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet("background: #1e1e2e; color: #cdd6f4;");

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // ── Title bar ─────────────────────────────────────────────────────────────
    auto* titleBar = new QWidget;
    titleBar->setFixedHeight(44);
    titleBar->setStyleSheet("background: #2a2a3c; border-bottom: 1px solid #313244;");
    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(12, 0, 12, 0);
    titleLayout->setSpacing(8);

    auto* backBtn = new QPushButton("← Back to Editor");
    backBtn->setFixedHeight(28);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; font-size: 12px; padding: 0 12px; }"
        "QPushButton:hover { background: #45475a; }");
    connect(backBtn, &QPushButton::clicked, this, &ErrorJournal::backToEditor);
    titleLayout->addWidget(backBtn);

    titleLayout->addStretch();

    auto* titleLabel = new QLabel("Error Journal");
    titleLabel->setStyleSheet("color: #cdd6f4; font-size: 14px; font-weight: bold;");
    titleLayout->addWidget(titleLabel);

    titleLayout->addStretch();

    auto* clearAllBtn = new QPushButton("Clear All");
    clearAllBtn->setFixedHeight(28);
    clearAllBtn->setCursor(Qt::PointingHandCursor);
    clearAllBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #f38ba8; border: 1px solid #45475a;"
        " border-radius: 4px; font-size: 12px; padding: 0 12px; }"
        "QPushButton:hover { background: #45475a; }");
    connect(clearAllBtn, &QPushButton::clicked, this, &ErrorJournal::onClearAll);
    titleLayout->addWidget(clearAllBtn);

    outerLayout->addWidget(titleBar);

    // ── Summary bar ───────────────────────────────────────────────────────────
    m_summaryLabel = new QLabel("No errors logged yet.");
    m_summaryLabel->setAlignment(Qt::AlignCenter);
    m_summaryLabel->setFixedHeight(32);
    m_summaryLabel->setStyleSheet(
        "QLabel { background: #181825; color: #a6adc8; font-size: 11px;"
        " border-bottom: 1px solid #313244; padding: 0 14px; }");
    outerLayout->addWidget(m_summaryLabel);

    // ── Search bar ────────────────────────────────────────────────────────────
    auto* searchBar = new QWidget;
    searchBar->setFixedHeight(40);
    searchBar->setStyleSheet("background: #1e1e2e; border-bottom: 1px solid #313244;");
    auto* searchLayout = new QHBoxLayout(searchBar);
    searchLayout->setContentsMargins(12, 6, 12, 6);
    searchLayout->setSpacing(8);

    auto* searchIcon = new QLabel("Search:");
    searchIcon->setStyleSheet("color: #a6adc8; font-size: 12px;");
    searchLayout->addWidget(searchIcon);

    m_searchInput = new QLineEdit;
    m_searchInput->setPlaceholderText("Filter by error type, file, or message...");
    m_searchInput->setFixedHeight(26);
    m_searchInput->setStyleSheet(
        "QLineEdit { background: #2a2a3c; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; font-size: 12px; padding: 0 8px; }"
        "QLineEdit:focus { border-color: #89b4fa; }");
    connect(m_searchInput, &QLineEdit::textChanged, this, &ErrorJournal::onSearch);
    searchLayout->addWidget(m_searchInput, 1);

    outerLayout->addWidget(searchBar);

    // ── List widget ───────────────────────────────────────────────────────────
    m_list = new QListWidget;
    m_list->setStyleSheet(
        "QListWidget { background: #1e1e2e; border: none; }"
        "QListWidget::item { border-bottom: 1px solid #313244;"
        " padding: 0; background: transparent; }"
        "QListWidget::item:selected { background: #313244; }"
        "QListWidget::item:hover { background: #252535; }"
        "QScrollBar:vertical { background: #181825; width: 8px; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background: #45475a; border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");
    connect(m_list, &QListWidget::itemClicked, this, &ErrorJournal::onItemClicked);
    outerLayout->addWidget(m_list, 1);
}

// ── Public interface ──────────────────────────────────────────────────────────

void ErrorJournal::setWorkspaceRoot(const QString& rootPath)
{
    m_workspaceRoot = rootPath;
    loadFromDisk();
    rebuildList();
}

void ErrorJournal::logError(const QString& rawOutput, const QString& filePath)
{
    // Split output into lines, find error lines
    QStringList lines = rawOutput.split('\n', Qt::SkipEmptyParts);

    // Collect all error-like lines
    QStringList errorLines;
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        // Python errors end with "Error:" pattern or start with "Traceback"
        if (trimmed.contains(QRegularExpression(
                "(?:Error|Exception|Warning|Traceback|SyntaxError|NameError"
                "|TypeError|ValueError|IndexError|KeyError|AttributeError"
                "|ImportError|RuntimeError|ZeroDivisionError)")) ||
            trimmed.startsWith("error:") ||
            trimmed.startsWith("warning:") ||
            trimmed.contains(": error:") ||
            trimmed.contains(": warning:")) {
            errorLines.append(trimmed);
        }
    }

    if (errorLines.isEmpty()) return;

    // Create one entry for the most significant error line
    // (last non-"Traceback" line tends to be the actual error)
    QString bestLine = errorLines.last();
    for (int i = errorLines.size() - 1; i >= 0; --i) {
        if (!errorLines[i].startsWith("Traceback") &&
            errorLines[i].contains(QRegularExpression("Error|Exception"))) {
            bestLine = errorLines[i];
            break;
        }
    }

    ErrorEntry entry;
    entry.timestamp  = QDateTime::currentDateTime();
    entry.message    = bestLine;
    entry.errorType  = extractErrorType(bestLine);
    entry.filePath   = filePath;
    entry.lineNumber = extractLineNumber(rawOutput);  // search whole output for line ref
    entry.fixed      = false;

    m_entries.prepend(entry);  // newest first
    saveToDisk();
    rebuildList(m_searchInput ? m_searchInput->text() : QString());
    emit errorCountChanged(unfixedCount());
}

void ErrorJournal::logSuccess(const QString& filePath, int /*linesChanged*/)
{
    bool changed = false;
    for (ErrorEntry& entry : m_entries) {
        if (!entry.fixed && entry.filePath == filePath) {
            entry.fixed = true;
            entry.fixDescription = "Fixed on next successful run";
            changed = true;
            break;  // mark only the most recent unfixed entry for this file
        }
    }
    if (changed) {
        saveToDisk();
        rebuildList(m_searchInput ? m_searchInput->text() : QString());
        emit errorCountChanged(unfixedCount());
    }
}

int ErrorJournal::unfixedCount() const
{
    int count = 0;
    for (const ErrorEntry& e : m_entries)
        if (!e.fixed) ++count;
    return count;
}

// ── Private slots ─────────────────────────────────────────────────────────────

void ErrorJournal::onSearch(const QString& text)
{
    rebuildList(text);
}

void ErrorJournal::onClearAll()
{
    m_entries.clear();
    saveToDisk();
    rebuildList();
    emit errorCountChanged(0);
}

void ErrorJournal::onItemClicked(QListWidgetItem* item)
{
    int idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= m_entries.size()) return;

    const ErrorEntry& entry = m_entries[idx];
    if (!entry.filePath.isEmpty() && entry.lineNumber > 0)
        emit jumpToFile(entry.filePath, entry.lineNumber);
}

// ── Private helpers ───────────────────────────────────────────────────────────

void ErrorJournal::loadFromDisk()
{
    m_entries.clear();
    QString path = journalPath();
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) return;

    const QJsonArray arr = doc.array();
    for (const QJsonValue& v : arr) {
        QJsonObject obj = v.toObject();
        ErrorEntry entry;
        entry.timestamp      = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
        entry.message        = obj["message"].toString();
        entry.errorType      = obj["errorType"].toString();
        entry.filePath       = obj["filePath"].toString();
        entry.lineNumber     = obj["lineNumber"].toInt();
        entry.fixDescription = obj["fixDescription"].toString();
        entry.fixed          = obj["fixed"].toBool();
        m_entries.append(entry);
    }
}

void ErrorJournal::saveToDisk()
{
    QString path = journalPath();
    if (path.isEmpty()) return;

    QJsonArray arr;
    for (const ErrorEntry& e : m_entries) {
        QJsonObject obj;
        obj["timestamp"]      = e.timestamp.toString(Qt::ISODate);
        obj["message"]        = e.message;
        obj["errorType"]      = e.errorType;
        obj["filePath"]       = e.filePath;
        obj["lineNumber"]     = e.lineNumber;
        obj["fixDescription"] = e.fixDescription;
        obj["fixed"]          = e.fixed;
        arr.append(obj);
    }

    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(arr).toJson());
    }
}

void ErrorJournal::rebuildList(const QString& filter)
{
    m_list->clear();

    // Build summary: most common error type
    QMap<QString, int> typeCounts;
    for (const ErrorEntry& e : m_entries)
        typeCounts[e.errorType]++;

    QString mostCommon;
    int maxCount = 0;
    for (auto it = typeCounts.begin(); it != typeCounts.end(); ++it) {
        if (it.value() > maxCount) {
            maxCount = it.value();
            mostCommon = it.key();
        }
    }

    if (m_entries.isEmpty()) {
        m_summaryLabel->setText("No errors logged yet.");
    } else {
        int unfixed = unfixedCount();
        if (!mostCommon.isEmpty() && maxCount > 1)
            m_summaryLabel->setText(
                QString("Your most common error: %1 (hit %2 times)  |  %3 unfixed error(s)")
                .arg(mostCommon).arg(maxCount).arg(unfixed));
        else
            m_summaryLabel->setText(
                QString("%1 error(s) logged  |  %2 unfixed")
                .arg(m_entries.size()).arg(unfixed));
    }

    // Populate list
    QString filterLower = filter.toLower();
    for (int i = 0; i < m_entries.size(); ++i) {
        const ErrorEntry& entry = m_entries[i];

        if (!filterLower.isEmpty()) {
            bool match = entry.message.toLower().contains(filterLower) ||
                         entry.errorType.toLower().contains(filterLower) ||
                         entry.filePath.toLower().contains(filterLower);
            if (!match) continue;
        }

        // Build item widget as a QListWidgetItem with custom data
        QString timeStr = entry.timestamp.toString("yyyy-MM-dd  HH:mm");
        QString fileStr = entry.filePath.isEmpty() ? "(no file)"
                         : QFileInfo(entry.filePath).fileName();
        if (entry.lineNumber > 0)
            fileStr += QString("  line %1").arg(entry.lineNumber);

        QString fixStr;
        if (entry.fixed)
            fixStr = QString("  Fixed: %1").arg(entry.fixDescription);

        // Truncate long messages
        QString msgDisplay = entry.message;
        if (msgDisplay.length() > 120)
            msgDisplay = msgDisplay.left(117) + "...";

        QString itemText = QString("%1  |  %2\n%3%4")
                           .arg(timeStr, msgDisplay, fileStr, fixStr);

        auto* item = new QListWidgetItem(itemText);
        item->setData(Qt::UserRole, i);

        // Colour: fixed = dim green accent, unfixed = red accent
        if (entry.fixed) {
            item->setForeground(QColor("#a6e3a1"));
        } else {
            item->setForeground(QColor("#f38ba8"));
        }

        QFont font;
        font.setPointSize(10);
        item->setFont(font);
        item->setSizeHint(QSize(0, 58));

        m_list->addItem(item);
    }
}

QString ErrorJournal::journalPath() const
{
    if (m_workspaceRoot.isEmpty()) return {};
    return QDir(m_workspaceRoot).filePath(".clarity-errors.json");
}

QString ErrorJournal::extractErrorType(const QString& msg)
{
    // Match "SomethingError:" or "SomethingException:"
    QRegularExpression re("\\b(\\w+(?:Error|Exception|Warning))\\b");
    auto m = re.match(msg);
    if (m.hasMatch()) return m.captured(1);

    // C/C++ style "error:" prefix
    if (msg.contains(": error:")) return "CompileError";
    if (msg.contains(": warning:")) return "CompileWarning";
    if (msg.startsWith("error:")) return "CompileError";

    return "Error";
}

int ErrorJournal::extractLineNumber(const QString& rawOutput)
{
    // Python: "  File "...", line N"
    {
        QRegularExpression re(R"(File ".*?", line (\d+))");
        QRegularExpressionMatchIterator it = re.globalMatch(rawOutput);
        int last = 0;
        while (it.hasNext()) {
            auto m = it.next();
            last = m.captured(1).toInt();
        }
        if (last > 0) return last;
    }
    // C/C++: "file.c:12:34: error:"
    {
        QRegularExpression re(R"(:\s*(\d+)\s*:\s*(?:\d+\s*:)?\s*(?:error|warning):)");
        auto m = re.match(rawOutput);
        if (m.hasMatch()) return m.captured(1).toInt();
    }
    // Generic ":N:" pattern
    {
        QRegularExpression re(R"(:(\d+):)");
        auto m = re.match(rawOutput);
        if (m.hasMatch()) return m.captured(1).toInt();
    }
    return 0;
}
