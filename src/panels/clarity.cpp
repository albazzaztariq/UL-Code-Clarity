#include "panels/clarity.h"
#include "core/theme.h"
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSettings>

using namespace Theme::Css;

// Shared theme flag so ClarityEntry can pick up the current theme when built
static bool s_isDark = true;

// ============================================================================
// ClarityEntry
// ============================================================================

ClarityEntry::ClarityEntry(const QString &title, const QString &time,
                           const QString &detailHtml, QWidget *parent)
    : QFrame(parent), m_expanded(false)
{
    setFrameShape(QFrame::NoFrame);
    setCursor(Qt::PointingHandCursor);
    if (s_isDark) {
        setStyleSheet(QString(
            "ClarityEntry { background: %1; border-radius: 6px;"
            " border: none; border-left: 3px solid %2; }"
            "ClarityEntry:hover { background: %3;"
            " border: none; border-left: 3px solid %2; }"
        ).arg(BG3, ACCENT, BG4));
    } else {
        setStyleSheet(
            "ClarityEntry { background: transparent; border-radius: 0;"
            " border: none; border-left: 3px solid #4f5b6a; margin-bottom: 8px; }"
            "ClarityEntry:hover { background: transparent;"
            " border: none; border-left: 3px solid #4f5b6a; margin-bottom: 8px; }");
    }

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(4);

    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setStyleSheet(s_isDark
        ? QString("font-size: 12px; font-weight: 600; color: %1; background: transparent;").arg(FG)
        : "font-size: 12px; font-weight: 600; color: #1e1e2e; background: transparent;");
    m_titleLabel->setWordWrap(true);
    layout->addWidget(m_titleLabel);

    m_timeLabel = new QLabel(time, this);
    m_timeLabel->setStyleSheet(s_isDark
        ? QString("font-size: 10px; color: %1; background: transparent;").arg(FG2)
        : "font-size: 10px; color: #666666; background: transparent;");
    layout->addWidget(m_timeLabel);

    // Detail section (hidden by default)
    m_detailWidget = new QWidget(this);
    m_detailWidget->setVisible(false);
    buildDetailContent(detailHtml);
    layout->addWidget(m_detailWidget);
}

void ClarityEntry::buildDetailContent(const QString &detailHtml)
{
    auto *layout = new QVBoxLayout(m_detailWidget);
    layout->setContentsMargins(8, 6, 8, 8);
    layout->setSpacing(4);

    m_detailWidget->setStyleSheet(s_isDark
        ? QString("background: %1; border-radius: 6px;").arg(BG)
        : "background: transparent;");

    auto *detailLabel = new QLabel(detailHtml, m_detailWidget);
    detailLabel->setStyleSheet(s_isDark
        ? QString("font-size: 11px; line-height: 1.5; color: %1; background: transparent;"
                  "QLabel a { color: %2; }").arg(FG2, ACCENT2)
        : "font-size: 11px; line-height: 1.5; color: #444444; background: transparent;"
          "QLabel a { color: #4f5b6a; }");
    detailLabel->setWordWrap(true);
    detailLabel->setTextFormat(Qt::RichText);
    detailLabel->setOpenExternalLinks(false);
    layout->addWidget(detailLabel);

    // "Try It Yourself" button (if the detail contains one)
    if (detailHtml.contains("Try It Yourself")) {
        auto *tryBtn = new QPushButton("Try It Yourself ->", m_detailWidget);
        tryBtn->setStyleSheet(QString(
            "QPushButton { background: %1; color: %2; font-weight: 600; "
            "padding: 3px 10px; border-radius: 4px; font-size: 10px; }"
            "QPushButton:hover { filter: brightness(1.15); }"
        ).arg(MAUVE, BG));
        tryBtn->setCursor(Qt::PointingHandCursor);
        tryBtn->setFixedHeight(22);
        connect(tryBtn, &QPushButton::clicked, this, &ClarityEntry::tryItYourselfClicked);
        layout->addWidget(tryBtn, 0, Qt::AlignLeft);
    }
}

void ClarityEntry::setExpanded(bool expanded)
{
    m_expanded = expanded;
    m_detailWidget->setVisible(expanded);
}

bool ClarityEntry::isExpanded() const
{
    return m_expanded;
}

void ClarityEntry::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    setExpanded(!m_expanded);
    emit clicked();
}

// ============================================================================
// ClarityPanel
// ============================================================================

ClarityPanel::ClarityPanel(QWidget *parent)
    : QWidget(parent), m_level(1)
{
    setStyleSheet(QString("background: %1;").arg(BG2));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Column header bar: +/− buttons top-right
    m_columnHeader = new QWidget(this);
    m_columnHeader->setStyleSheet(QString(
        "background: %1; border-bottom: 1px solid %2;"
    ).arg(BG2, BORDER));
    auto *headerBarLayout = new QHBoxLayout(m_columnHeader);
    headerBarLayout->setContentsMargins(8, 4, 8, 4);

    headerBarLayout->addStretch();

    // Faded + (non-functional when expanded)
    m_plusLabel = new QLabel("+", m_columnHeader);
    m_plusLabel->setFixedSize(26, 26);
    m_plusLabel->setAlignment(Qt::AlignCenter);
    m_plusLabel->setStyleSheet(QString(
        "font-size: 18px; font-weight: bold; color: #45475a; background: transparent;"));
    m_plusLabel->setToolTip("Panel is already open");
    headerBarLayout->addWidget(m_plusLabel);

    // − button (collapse)
    m_hideButton = new QPushButton(QString::fromUtf8("\xe2\x88\x92"), m_columnHeader);
    m_hideButton->setFixedSize(26, 26);
    m_hideButton->setCursor(Qt::PointingHandCursor);
    m_hideButton->setStyleSheet(QString(
        "QPushButton { color: %1; background: transparent;"
        " font-size: 18px; font-weight: bold; border: none; padding: 0; }"
        "QPushButton:hover { color: %2; background: %3; border-radius: 4px; }")
        .arg(FG, FG, BG4));
    m_hideButton->setToolTip("Collapse panel");
    connect(m_hideButton, &QPushButton::clicked, this, &ClarityPanel::closeRequested);
    headerBarLayout->addWidget(m_hideButton);

    mainLayout->addWidget(m_columnHeader);

    // Changelog content area with padding
    auto *contentWidget = new QWidget(this);
    m_contentWidget = contentWidget;
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(8, 8, 8, 8);
    contentLayout->setSpacing(0);

    m_headerLabel = new QLabel("CHANGE LOG", contentWidget);
    m_headerLabel->setStyleSheet(QString(
        "font-size: 10px; font-weight: 700; text-transform: uppercase; "
        "letter-spacing: 1px; color: %1; padding: 4px 4px 8px;"
    ).arg(FG2));
    contentLayout->addWidget(m_headerLabel);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(QString(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 7px; background: transparent; }"
        "QScrollBar::handle:vertical { background: %1; border-radius: 4px; }")
        .arg(BG4));

    auto *scrollWidget = new QWidget;
    m_entriesLayout = new QVBoxLayout(scrollWidget);
    m_entriesLayout->setContentsMargins(0, 0, 4, 0);
    m_entriesLayout->setSpacing(8);
    m_entriesLayout->addStretch();
    m_scrollArea->setWidget(scrollWidget);

    contentLayout->addWidget(m_scrollArea);
    mainLayout->addWidget(contentWidget, 1);

    // Populate with sample entries only on first launch
    QSettings s("CodeClarity", "CodeClarity");
    if (!s.value("hasSeenExamples", false).toBool()) {
        rebuildEntries();
    }
}

void ClarityPanel::toggleCollapsed()
{
    m_collapsed = !m_collapsed;

    if (m_contentWidget)
        m_contentWidget->setVisible(!m_collapsed);

    auto *headerLayout = qobject_cast<QHBoxLayout*>(m_columnHeader->layout());

    if (m_collapsed) {
        // Show only + button to expand
        m_hideButton->setText("+");
        m_hideButton->setToolTip("Expand panel");
        if (m_plusLabel) m_plusLabel->hide();
        if (headerLayout) headerLayout->setContentsMargins(4, 4, 4, 4);
    } else {
        // Show faded + and active −
        m_hideButton->setText(QString::fromUtf8("\xe2\x88\x92"));  // −
        m_hideButton->setToolTip("Collapse panel");
        if (m_plusLabel) m_plusLabel->show();
        if (headerLayout) headerLayout->setContentsMargins(8, 4, 8, 4);
    }

    QWidget *container = parentWidget();
    if (container) {
        if (m_collapsed) {
            container->setMinimumWidth(36);
            container->setMaximumWidth(36);
        } else {
            container->setMinimumWidth(180);
            container->setMaximumWidth(600);
        }
    }
}

void ClarityPanel::setLevel(int level)
{
    if (level < 1) level = 1;
    if (level > 4) level = 4;
    m_level = level;
    rebuildEntries();
}

int ClarityPanel::level() const
{
    return m_level;
}

void ClarityPanel::addEntry(const ClarityEntryData &data)
{
    auto *entry = new ClarityEntry(data.title, data.time, data.detailHtml,
                                   m_scrollArea->widget());
    int idx = m_entries.size();
    connect(entry, &ClarityEntry::clicked, this, [this, idx]() {
        emit entryClicked(idx);
    });
    connect(entry, &ClarityEntry::tryItYourselfClicked, this, [this, idx]() {
        emit tryItYourself(idx);
    });

    // Insert before the stretch
    m_entriesLayout->insertWidget(m_entriesLayout->count() - 1, entry);
    m_entries.append(entry);
}

void ClarityPanel::clearEntries()
{
    for (auto *entry : m_entries) {
        m_entriesLayout->removeWidget(entry);
        entry->deleteLater();
    }
    m_entries.clear();
}

void ClarityPanel::markExamplesSeen()
{
    QSettings s("CodeClarity", "CodeClarity");
    if (!s.value("hasSeenExamples", false).toBool()) {
        // Note: hasSeenExamples is written by AIChatPanel::markExamplesSeen
        // which is the single source of truth. Here we just clear entries.
        clearEntries();
    }
}

void ClarityPanel::rebuildEntries()
{
    clearEntries();
    auto entries = sampleEntries(m_level);
    for (const auto &e : entries) {
        addEntry(e);
    }
}

// ============================================================================
// Sample entries per level -- ported from prototype CLARITY_ENTRIES
// ============================================================================

void ClarityPanel::applyTheme(bool isDark)
{
    m_isDark = isDark;
    s_isDark = isDark;  // shared so ClarityEntry constructors pick it up
    if (isDark) {
        setStyleSheet(QString("background: %1;").arg(BG2));
        m_columnHeader->setStyleSheet(QString(
            "background: %1; border-bottom: 1px solid %2;"
        ).arg(BG2, BORDER));
        m_hideButton->setStyleSheet(QString(
            "QPushButton { color: %1; background: transparent;"
            " font-size: 18px; font-weight: bold; border: none; padding: 0; }"
            "QPushButton:hover { color: %2; background: %3; border-radius: 4px; }")
            .arg(FG, FG, BG4));
        m_headerLabel->setStyleSheet(QString(
            "font-size: 10px; font-weight: 700; text-transform: uppercase; "
            "letter-spacing: 1px; color: %1; padding: 4px 4px 8px;"
        ).arg(FG2));
        m_scrollArea->setStyleSheet(QString(
            "QScrollArea { border: none; background: transparent; }"
            "QScrollBar:vertical { width: 7px; background: transparent; }"
            "QScrollBar::handle:vertical { background: %1; border-radius: 4px; }")
            .arg(BG4));
    } else {
        setStyleSheet("background: #fafafa;");
        m_columnHeader->setStyleSheet(
            "background: #fafafa; border-bottom: 1px solid #e0e0e0;");
        m_hideButton->setStyleSheet(
            "QPushButton { color: #4b515a; background: transparent;"
            " font-size: 18px; font-weight: bold; border: none; padding: 0; }"
            "QPushButton:hover { color: #2f343b; background: #d6dbe3; border-radius: 4px; }");
        m_headerLabel->setStyleSheet(
            "font-size: 10px; font-weight: 700; text-transform: uppercase; "
            "letter-spacing: 1px; color: #666666; padding: 4px 4px 8px;");
        m_scrollArea->setStyleSheet(
            "QScrollArea { border: none; background: transparent; }"
            "QScrollBar:vertical { width: 7px; background: #f0f0f0; }"
            "QScrollBar::handle:vertical { background: #cccccc; border-radius: 4px; }"
        );
    }
    // Rebuild entries to pick up the new theme colors
    rebuildEntries();
}

QVector<ClarityEntryData> ClarityPanel::sampleEntries(int level)
{
    QVector<ClarityEntryData> entries;

    switch (level) {
    case 1: // Beginner -- plain English, concepts explained
        entries.append({
            "Created a function that calculates total price",
            "2 min ago -- AI generated",
            "<h4 style='color:#89b4fa;margin:6px 0 3px;font-size:11px'>What Changed</h4>"
            "<p>Added a <code>calculate_total</code> function that takes a list of prices "
            "and a tax rate, returns the total with tax.</p>"
            "<h4 style='color:#89b4fa;margin:6px 0 3px;font-size:11px'>Why</h4>"
            "<p>You asked \"make a function to calculate total price with tax.\" "
            "The AI multiplies the sum by (1 + tax_rate).</p>"
            "<h4 style='color:#89b4fa;margin:6px 0 3px;font-size:11px'>Concepts Used</h4>"
            "<p><a href='#'>Functions</a> -- reusable blocks of code<br>"
            "<a href='#'>Parameters</a> -- values passed in<br>"
            "<a href='#'>Return values</a> -- what comes back</p>"
            "<h4 style='color:#89b4fa;margin:6px 0 3px;font-size:11px'>What You'd Need to Know</h4>"
            "<p>How to define a function with <code>def</code>, use <code>sum()</code>, "
            "and return a value.</p>"
            "<p>Try It Yourself</p>"
        });
        entries.append({
            "Fixed the loop that was skipping the last item",
            "5 min ago -- AI fix",
            "<h4 style='color:#89b4fa;margin:6px 0 3px;font-size:11px'>What Changed</h4>"
            "<p>Changed <code>range(len(items) - 1)</code> to <code>range(len(items))</code>.</p>"
            "<h4 style='color:#89b4fa;margin:6px 0 3px;font-size:11px'>Why</h4>"
            "<p><code>range()</code> already excludes the upper bound. Subtracting 1 "
            "skipped the last item.</p>"
            "<h4 style='color:#89b4fa;margin:6px 0 3px;font-size:11px'>Concepts Used</h4>"
            "<p><a href='#'>Off-by-one errors</a> -- a common counting mistake</p>"
            "<p>Try It Yourself</p>"
        });
        entries.append({
            "Added error handling for empty price lists",
            "8 min ago -- AI generated",
            "<h4 style='color:#89b4fa;margin:6px 0 3px;font-size:11px'>What Changed</h4>"
            "<p>Added a guard clause returning 0.0 if the list is empty.</p>"
            "<h4 style='color:#89b4fa;margin:6px 0 3px;font-size:11px'>Why</h4>"
            "<p>Prevents silent failures on empty input.</p>"
            "<h4 style='color:#89b4fa;margin:6px 0 3px;font-size:11px'>Concepts Used</h4>"
            "<p><a href='#'>Guard clauses</a> -- early returns that simplify logic</p>"
            "<p>Try It Yourself</p>"
        });
        break;

    case 2: // Intermediate -- less hand-holding, more technical
        entries.append({
            "Added calculate_total(prices, tax_rate) -> float",
            "2 min ago",
            "<h4 style='color:#89b4fa;margin:6px 0 3px;font-size:11px'>What Changed</h4>"
            "<p>New function: sums list, applies tax multiplier, returns rounded float.</p>"
            "<h4 style='color:#89b4fa;margin:6px 0 3px;font-size:11px'>Why</h4>"
            "<p>User request. Standard pattern: accumulate -> transform -> return.</p>"
            "<h4 style='color:#89b4fa;margin:6px 0 3px;font-size:11px'>Concepts</h4>"
            "<p>Function definition, default parameters, sum + multiply pattern</p>"
        });
        entries.append({
            "Fixed off-by-one in range(len(items) - 1)",
            "5 min ago",
            "<h4 style='color:#89b4fa;margin:6px 0 3px;font-size:11px'>What Changed</h4>"
            "<p>Removed erroneous <code>- 1</code> from range call. range() is already exclusive.</p>"
        });
        entries.append({
            "Guard clause: return 0.0 on empty list",
            "8 min ago",
            "<h4 style='color:#89b4fa;margin:6px 0 3px;font-size:11px'>What Changed</h4>"
            "<p>Early return prevents division-by-zero downstream.</p>"
        });
        break;

    case 3: // Developer -- terse, technical lingo
        entries.append({
            "calculate_total(): sum + tax multiplier, default 10%",
            "2 min ago",
            "<p>Signature: <code>(list[float], float=0.10) -> float</code>. Rounds to 2dp.</p>"
        });
        entries.append({
            "Fix: range(len-1) -> range(len), off-by-one",
            "5 min ago",
            "<p>range() upper bound is exclusive. The -1 was double-excluding.</p>"
        });
        entries.append({
            "Guard: early return on empty prices",
            "8 min ago",
            "<p>Prevents ZeroDivisionError if prices is empty.</p>"
        });
        break;

    case 4: // No Assist -- raw verbose changelog
        entries.append({
            "def calculate_total(prices, tax_rate=0.10) -> float",
            "2 min ago",
            "<p>+L1-22 main.py: added function calculate_total(list, float). "
            "Body: sum accumulator loop over prices[i], multiply by (1 + tax_rate), "
            "round(result, 2). Default param tax_rate=0.10.</p>"
        });
        entries.append({
            "changed range(len(items) - 1) to range(len(items))",
            "5 min ago",
            "<p>~L7 main.py: modified range() upper bound. Removed arithmetic subtraction "
            "on len() call. Affected iteration count: n-1 -> n.</p>"
        });
        entries.append({
            "added guard clause L2-3: if not prices return 0.0",
            "8 min ago",
            "<p>+L2-3 calculate_total: inserted conditional early return. "
            "Predicate: falsy check on prices param. Return value: float literal 0.0.</p>"
        });
        break;
    }

    return entries;
}
