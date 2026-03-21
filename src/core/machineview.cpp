#include "core/machineview.h"
#include "core/jsonloader.h"

#include <QHBoxLayout>
#include <QFrame>
#include <QRegularExpression>
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>

// ============================================================================
// MachineViewPanel — constructor
// ============================================================================

MachineViewPanel::MachineViewPanel(QWidget* parent)
    : QWidget(parent)
{
    loadPatterns();

    setFixedHeight(180);
    setStyleSheet("background: #181825; border-top: 1px solid #313244;");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────────
    auto* header = new QWidget;
    header->setFixedHeight(32);
    header->setStyleSheet("background: #181825; border-bottom: 1px solid #313244;");
    auto* hLayout = new QHBoxLayout(header);
    hLayout->setContentsMargins(12, 0, 8, 0);
    hLayout->setSpacing(8);

    m_titleLabel = new QLabel("Machine View");
    m_titleLabel->setStyleSheet("color: #cdd6f4; font-size: 12px; font-weight: bold;"
                                " background: transparent;");
    hLayout->addWidget(m_titleLabel);

    m_lineLabel = new QLabel;
    m_lineLabel->setStyleSheet("color: #585b70; font-size: 11px; background: transparent;");
    hLayout->addWidget(m_lineLabel);

    hLayout->addStretch();

    auto* hintLabel = new QLabel("Click any line in the editor");
    hintLabel->setStyleSheet("color: #45475a; font-size: 11px; background: transparent;");
    hLayout->addWidget(hintLabel);

    m_closeBtn = new QPushButton("x");
    m_closeBtn->setFixedSize(20, 20);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(
        "QPushButton { background: none; color: #585b70; font-size: 13px; border: none; }"
        "QPushButton:hover { color: #f38ba8; }");
    hLayout->addWidget(m_closeBtn);

    mainLayout->addWidget(header);

    // ── Content area ──────────────────────────────────────────────────────────
    auto* content = new QWidget;
    content->setStyleSheet("background: #1e1e2e;");
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(16, 10, 16, 10);
    contentLayout->setSpacing(6);

    m_emptyLabel = new QLabel("Select a line above to see what the computer does.");
    m_emptyLabel->setStyleSheet("color: #45475a; font-size: 12px; background: transparent;");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(m_emptyLabel);

    m_headLabel = new QLabel;
    m_headLabel->setStyleSheet("color: #89b4fa; font-size: 13px; font-weight: bold;"
                               " background: transparent;");
    m_headLabel->setWordWrap(true);
    m_headLabel->hide();
    contentLayout->addWidget(m_headLabel);

    m_bodyLabel = new QLabel;
    m_bodyLabel->setStyleSheet("color: #cdd6f4; font-size: 12px; background: transparent;"
                               " line-height: 1.4;");
    m_bodyLabel->setWordWrap(true);
    m_bodyLabel->hide();
    contentLayout->addWidget(m_bodyLabel);

    contentLayout->addStretch();
    mainLayout->addWidget(content, 1);

    connect(m_closeBtn, &QPushButton::clicked, this, &MachineViewPanel::closeRequested);
}

// ============================================================================
// Load patterns from JSON
// ============================================================================

void MachineViewPanel::loadPatterns()
{
    QJsonArray arr = JsonLoader::loadArray("machineview.json", "patterns");
    for (const QJsonValue& pv : arr) {
        QJsonObject po = pv.toObject();
        PatternEntry entry;
        entry.id = po["id"].toString();
        entry.language = po["language"].toString(); // "" = any language

        // Match criteria
        for (const QJsonValue& v : po["match_starts_with"].toArray())
            entry.startsWithAny << v.toString();
        for (const QJsonValue& v : po["match_contains"].toArray())
            entry.containsAny << v.toString();
        for (const QJsonValue& v : po["match_contains_any"].toArray())
            entry.containsAny << v.toString();
        for (const QJsonValue& v : po["exclude_starts_with"].toArray())
            entry.excludeStartsWith << v.toString();
        for (const QJsonValue& v : po["exclude_contains"].toArray())
            entry.excludeContains << v.toString();

        if (po.contains("match_regex"))
            entry.matchRegex = po["match_regex"].toString();

        entry.requireAssignment      = po["require_assignment"].toBool(false);
        entry.requireStringHint      = po["require_string_hint"].toBool(false);
        entry.excludeStrings         = po["exclude_strings"].toBool(false);
        entry.matchContainsStarOrAmp = po["match_contains_star_or_amp"].toBool(false);
        entry.requireAssignOrArrow   = po["require_assign_or_arrow"].toBool(false);
        entry.matchContainsBrackets  = po["match_contains_brackets"].toBool(false);
        entry.excludeIfNoAssignAndHasParen = po["exclude_if_contains_no_assign_and_has_paren"].toBool(false);

        // Text levels: levels.python.1..4 and levels.c.1..4
        QJsonObject levels = po["levels"].toObject();
        for (const QString& langKey : levels.keys()) {
            QJsonObject langObj = levels[langKey].toObject();
            for (const QString& levelKey : langObj.keys()) {
                QJsonObject slot = langObj[levelKey].toObject();
                LevelText lt;
                lt.headline = slot["headline"].toString();
                lt.body     = slot["body"].toString();
                entry.levels[langKey][levelKey.toInt()] = lt;
            }
        }

        m_patterns.append(entry);
    }
}

// ============================================================================
// Public interface
// ============================================================================

void MachineViewPanel::setLevel(int level)    { m_level = qBound(1, level, 4); }
void MachineViewPanel::setLanguage(const QString& lang) { m_lang = lang.toLower(); }
void MachineViewPanel::applyTheme(bool isDark) { m_isDark = isDark; }

void MachineViewPanel::clearExplanation()
{
    m_lineLabel->clear();
    m_headLabel->hide();
    m_bodyLabel->hide();
    m_emptyLabel->show();
}

void MachineViewPanel::explainLine(int lineNumber, const QString& lineText)
{
    QString trimmed = lineText.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith("//") || trimmed.startsWith('#')
        || trimmed.startsWith("/*") || trimmed == "{" || trimmed == "}") {
        clearExplanation();
        m_lineLabel->setText(QString("Line %1 — no operation").arg(lineNumber));
        return;
    }

    m_lineLabel->setText(QString("Line %1").arg(lineNumber));

    MachineExplanation expl = generateExplanation(trimmed, m_lang, m_level);

    if (expl.isEmpty()) {
        m_headLabel->hide();
        m_bodyLabel->hide();
        m_emptyLabel->setText(
            QString("Line %1: \"%2\"\n\nNo detailed explanation available for this pattern.")
                .arg(lineNumber)
                .arg(trimmed.left(60)));
        m_emptyLabel->show();
        return;
    }

    m_emptyLabel->hide();
    m_headLabel->setText(expl.headline);
    m_headLabel->show();
    m_bodyLabel->setText(expl.body);
    m_bodyLabel->show();
}

// ============================================================================
// Pattern dispatch — data-driven from loaded JSON patterns
// ============================================================================

MachineExplanation MachineViewPanel::generateExplanation(const QString& line,
                                                          const QString& lang,
                                                          int level) const
{
    for (const PatternEntry& entry : m_patterns) {
        // Language filter
        if (!entry.language.isEmpty() && entry.language != lang)
            continue;

        // Match criteria
        bool matched = false;

        if (!entry.startsWithAny.isEmpty()) {
            for (const QString& sw : entry.startsWithAny) {
                if (line.startsWith(sw)) { matched = true; break; }
            }
        }

        if (!matched && !entry.containsAny.isEmpty()) {
            for (const QString& c : entry.containsAny) {
                if (line.contains(c)) { matched = true; break; }
            }
        }

        if (!matched && !entry.matchRegex.isEmpty()) {
            static QHash<QString, QRegularExpression> reCache;
            if (!reCache.contains(entry.matchRegex))
                reCache[entry.matchRegex] = QRegularExpression(entry.matchRegex);
            if (reCache[entry.matchRegex].match(line).hasMatch())
                matched = true;
        }

        if (!matched && entry.matchContainsStarOrAmp) {
            bool hasStar = line.contains('*') && !line.contains("/*") && !line.contains("//");
            bool hasAmp  = line.contains('&');
            if (hasStar || hasAmp) matched = true;
        }

        if (!matched && entry.matchContainsBrackets) {
            if (line.contains('[') && line.contains(']')) matched = true;
        }

        if (!matched) continue;

        // Exclusions
        bool excluded = false;
        for (const QString& sw : entry.excludeStartsWith) {
            if (line.startsWith(sw)) { excluded = true; break; }
        }
        if (!excluded) {
            for (const QString& c : entry.excludeContains) {
                if (line.contains(c)) { excluded = true; break; }
            }
        }
        if (excluded) continue;

        // Extra conditions
        if (entry.requireAssignment && !line.contains('=')) continue;
        if (entry.requireStringHint) {
            bool hasStr = line.contains('"') || line.contains('\'') || line.contains("str(") || line.contains(".join(");
            if (!hasStr) continue;
        }
        if (entry.excludeStrings && (line.contains('"') || line.contains('\''))) continue;
        if (entry.requireAssignOrArrow && !line.contains('=') && !line.contains("->")) continue;
        if (entry.excludeIfNoAssignAndHasParen && line.contains('(') && !line.contains('=')) continue;

        // Look up text for this language + level
        QString langKey = lang;
        if (!entry.levels.contains(langKey)) {
            // Try generic fallback
            if (entry.levels.isEmpty()) continue;
            langKey = entry.levels.keys().first();
        }

        const QHash<int, LevelText>& levelMap = entry.levels[langKey];
        int lvl = level;
        if (!levelMap.contains(lvl)) {
            if (levelMap.isEmpty()) continue;
            lvl = levelMap.keys().first();
        }

        const LevelText& lt = levelMap[lvl];
        return { lt.headline, lt.body };
    }

    return {};
}
