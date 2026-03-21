#include "core/runtimestrip.h"
#include "core/theme.h"
#include "core/tutorial.h"

#include <QToolTip>
#include <QCursor>
#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

RuntimeStrip::RuntimeStrip(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(26);
    setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;").arg(Theme::Colors::bg2(), Theme::Colors::border()));

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(12, 0, 12, 0);
    m_layout->setSpacing(4);

    // ── Define runtime data for each language ───────────────────────
    m_runtimeData["Python 3.12"] = {
        {"Language",  "Python 3.12",                  {}, "", ""},
        {"Runtime",   "CPython (interpreter)",         {}, "CPython",
         "The standard Python interpreter. Code is compiled to bytecode then interpreted."},
        {"Memory",    "Garbage Collected (automatic)",  {}, "GC",
         "Python automatically tracks objects and frees memory when they're no longer used."},
        {"Threading", "GIL (one thread at a time)",     {}, "GIL",
         "The Global Interpreter Lock means only one thread runs Python code at a time."},
        {"VM",        "Bytecode -> interpreted",        {}, "Bytecode",
         "Your .py is compiled to .pyc bytecode, then the CPython VM interprets it."},
    };

    m_runtimeData["C (gcc)"] = {
        {"Language",    "C (compiled)",                {}, "", ""},
        {"Runtime",     "Native binary (no interpreter)", {}, "", ""},
        {"Memory",      "Manual (malloc/free)",         {}, "Manual Memory",
         "You allocate with malloc() and free with free(). Forget to free = memory leak."},
        {"Threading",   "pthreads (OS-level threads)",  {}, "pthreads",
         "C uses OS threads directly. No GIL — threads run truly in parallel."},
        {"Compiled by", "gcc",                          {}, "", ""},
    };

    m_runtimeData["C++ (g++)"] = {
        {"Language",    "C++ (compiled)",               {}, "", ""},
        {"Runtime",     "Native binary (no interpreter)", {}, "", ""},
        {"Memory",      "Manual + RAII",                {}, "RAII",
         "Manual allocation plus RAII: destructors automatically clean up when objects go out of scope."},
        {"Threading",   "std::thread / pthreads",       {}, "Threads",
         "C++ provides std::thread on top of OS threads. No GIL — true parallelism."},
        {"Objects",     "Constructors, destructors, vtable", {}, "OOP",
         "C++ has constructors, destructors, virtual functions with vtable dispatch."},
        {"Compiled by", "g++",                          {}, "", ""},
    };

    m_runtimeData["Rust (rustc)"] = {
        {"Language",    "Rust (compiled)",              {}, "", ""},
        {"Runtime",     "Native binary (no interpreter)", {}, "", ""},
        {"Memory",      "Ownership model (compiler-enforced)", {}, "Ownership",
         "Rust's borrow checker enforces memory safety at compile time. No GC, no manual free."},
        {"Threading",   "Safe concurrency (no data races)", {}, "Concurrency",
         "Rust prevents data races at compile time. Send and Sync traits guarantee thread safety."},
        {"Compiled by", "rustc",                        {}, "", ""},
    };

    // UL — tunable fields use options instead of value
    m_runtimeData["UniLogic"] = {
        {"Memory",      "",  {"gc", "manual", "refcount", "arena"}, "Memory Model",
         "gc = automatic garbage collection. manual = you call memtake/memgive. refcount = reference counting. arena = bulk allocate, free all at once."},
        {"Safety",      "",  {"checked", "unchecked", "memory_safe"}, "Safety Level",
         "checked = compiler catches errors. unchecked = no checks, max speed. memory_safe = full bounds + null safety."},
        {"Types",       "",  {"strict", "dynamic", "gradual"}, "Type Strictness",
         "strict = all types at compile time. dynamic = checked at runtime. gradual = mix."},
        {"Concurrency", "",  {"threaded", "parallel", "async", "cooperative"}, "Concurrency",
         "threaded = OS threads. parallel = data parallelism. async = coroutines. cooperative = manual yield."},
        {"Target",      "",  {"C (native)", "Python", "JavaScript", "WASM", "Bytecode VM"}, "Target",
         "Same source, multiple outputs. C = native binary. Python/JS = transpile. WASM = browser."},
    };

    m_currentLang = "Python 3.12";
    rebuild();
}

void RuntimeStrip::setLanguage(const QString& langDisplay)
{
    if (m_currentLang == langDisplay) return;
    m_currentLang = langDisplay;
    rebuild();
}

void RuntimeStrip::applyTheme(bool isDark)
{
    m_isDark = isDark;
    if (isDark) {
        setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;").arg(Theme::Colors::bg2(), Theme::Colors::border()));
    } else {
        setStyleSheet("background: #f0f0f0; border-bottom: 1px solid #d0d0d0;");  // light theme literals
    }
    rebuild();
}

void RuntimeStrip::toggle()
{
    m_visible = !m_visible;
    setVisible(m_visible);
}

void RuntimeStrip::show()
{
    m_visible = true;
    QWidget::show();
}

void RuntimeStrip::hide()
{
    m_visible = false;
    QWidget::hide();
}

void RuntimeStrip::rebuild()
{
    // Clear existing widgets
    while (m_layout->count() > 0) {
        QLayoutItem* item = m_layout->takeAt(0);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    auto it = m_runtimeData.find(m_currentLang);
    if (it == m_runtimeData.end()) {
        // Fallback to Python
        it = m_runtimeData.find("Python 3.12");
    }

    const auto& rows = it.value();
    for (int i = 0; i < rows.size(); ++i) {
        addItem(m_layout, rows[i]);
        if (i < rows.size() - 1) {
            addSeparator(m_layout);
        }
    }

    // Dismiss button
    m_layout->addStretch();
    auto* dismiss = new QPushButton(QChar(0x2715));  // ✕
    dismiss->setFixedSize(16, 16);
    if (m_isDark) {
        dismiss->setStyleSheet(QString(
            "QPushButton { color: %1; background: transparent;"
            " border: none; font-size: 12px; padding: 0; }"
            "QPushButton:hover { color: %2; }").arg(Theme::Colors::fg3(), Theme::Colors::fg()));
    } else {
        dismiss->setStyleSheet(
            "QPushButton { color: #999999; background: transparent;"
            " border: none; font-size: 12px; padding: 0; }"
            "QPushButton:hover { color: #1e1e2e; }");  // light theme literals
    }
    connect(dismiss, &QPushButton::clicked, this, &RuntimeStrip::toggle);
    m_layout->addWidget(dismiss);
}

void RuntimeStrip::addItem(QHBoxLayout* layout, const RuntimeRow& row)
{
    // Add directly to the strip layout — no wrapper QWidget so there are no
    // opaque child backgrounds that create blocky segments.

    // Label
    auto* label = new QLabel(row.label + ":");
    if (m_isDark) {
        label->setStyleSheet(QString("QLabel { color: %1; font-size: 10px; background: transparent; }").arg(Theme::Colors::fg2()));
    } else {
        label->setStyleSheet("QLabel { color: #666666; font-size: 10px; background: transparent; }");
    }
    layout->addWidget(label);

    if (row.options.isEmpty()) {
        // Static value
        auto* value = new QLabel(row.value);
        if (m_isDark) {
            value->setStyleSheet(QString("QLabel { color: %1; font-weight: 600; font-size: 10px; background: transparent; }").arg(Theme::Colors::fg()));
        } else {
            value->setStyleSheet("QLabel { color: #333333; font-weight: 600; font-size: 10px; background: transparent; }");
        }
        layout->addWidget(value);
    } else {
        // Tunable combo box
        auto* combo = new QComboBox;
        combo->addItems(row.options);
        if (m_isDark) {
            combo->setStyleSheet(QString(
                "QComboBox { background: transparent; color: %1;"
                " border: 1px solid %2; border-radius: 3px;"
                " padding: 1px 4px; font-size: 10px; font-weight: 600; }"
                "QComboBox::drop-down { border: none; background: transparent; }"
                "QComboBox QAbstractItemView { background: #3c3c54; color: %3;"
                " border: none; outline: none; selection-background-color: %2; }")
                .arg(Theme::Colors::yellow(), Theme::Colors::border(), Theme::Colors::fg()));
        } else {
            combo->setStyleSheet(
                "QComboBox { background: transparent; color: #333333;"
                " border: 1px solid #cccccc; border-radius: 3px;"
                " padding: 1px 4px; font-size: 10px; font-weight: 600; }"
                "QComboBox::drop-down { border: none; background: transparent; }"
                "QComboBox QAbstractItemView { background: #ffffff; color: #333333;"
                " border: none; outline: none; selection-background-color: #d0e0ff;"
                " selection-color: #1e1e2e; }");
        }
        combo->setFixedHeight(18);
        layout->addWidget(combo);
    }

    // Help button (if has help text)
    if (!row.helpTitle.isEmpty()) {
        auto* helpBtn = new QPushButton("?");
        helpBtn->setFixedSize(24, 24);
        if (m_isDark) {
            helpBtn->setStyleSheet(QString(
                "QPushButton { background: transparent; color: %1;"
                " border: none; font-size: 14px; padding: 0; }"
                "QPushButton:hover { color: %2; }").arg(Theme::Colors::fg2(), Theme::Colors::fg()));
        } else {
            helpBtn->setStyleSheet(
                "QPushButton { background: transparent; color: #888888;"
                " border: none; font-size: 14px; padding: 0; }"
                "QPushButton:hover { color: #333333; }");
        }
        QString title = row.helpTitle;
        QString body = row.helpBody;
        bool isMemory = (title == "Memory Model" || title == "GC" || title == "RAII"
                         || title == "Manual Memory" || title == "Ownership");
        connect(helpBtn, &QPushButton::clicked, this, [this, title, body, isMemory]() {
            auto* dlg = new QDialog(this);
            dlg->setWindowTitle(title);
            dlg->setModal(true);
            dlg->setMinimumWidth(380);
            dlg->setStyleSheet(QString("background: %1;").arg(Theme::Colors::bg()));
            auto* lay = new QVBoxLayout(dlg);
            lay->setContentsMargins(20, 16, 20, 16);
            lay->setSpacing(12);
            auto* desc = new QLabel(body);
            desc->setWordWrap(true);
            desc->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; }").arg(Theme::Colors::fg()));
            lay->addWidget(desc);
            auto* btnRow = new QHBoxLayout;
            btnRow->setSpacing(8);
            if (isMemory) {
                auto* launchBtn = new QPushButton("Launch Tutorial");
                launchBtn->setStyleSheet(QString(
                    "QPushButton { background: %1; color: %2; border: none;"
                    " border-radius: 6px; padding: 0 14px; font-size: 13px; font-weight: bold; min-height: 28px; }"
                    "QPushButton:hover { background: #b4d0fb; }")
                    .arg(Theme::Colors::accent(), Theme::Colors::bg()));
                launchBtn->setCursor(Qt::PointingHandCursor);
                connect(launchBtn, &QPushButton::clicked, dlg, [dlg, this]() {
                    dlg->accept();
                    auto* tut = TutorialDialog::memory(this);
                    tut->exec();
                    tut->deleteLater();
                });
                btnRow->addWidget(launchBtn);
            }
            btnRow->addStretch();
            auto* closeBtn = new QPushButton("Close");
            closeBtn->setStyleSheet(QString(
                "QPushButton { background: #313244; color: %1; border: 1px solid %2;"
                " border-radius: 6px; padding: 0 12px; font-size: 13px; min-height: 28px; }"
                "QPushButton:hover { background: %2; }")
                .arg(Theme::Colors::fg(), Theme::Colors::border()));
            closeBtn->setCursor(Qt::PointingHandCursor);
            connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
            btnRow->addWidget(closeBtn);
            lay->addLayout(btnRow);
            dlg->exec();
            dlg->deleteLater();
        });
        layout->addWidget(helpBtn);
    }
}

void RuntimeStrip::addSeparator(QHBoxLayout* layout)
{
    auto* sep = new QLabel("|");
    if (m_isDark) {
        sep->setStyleSheet(QString("QLabel { color: %1; font-size: 10px; padding: 0 2px; background: transparent; }").arg(Theme::Colors::border()));
    } else {
        sep->setStyleSheet("QLabel { color: #cccccc; font-size: 10px; padding: 0 2px; background: transparent; }");
    }
    layout->addWidget(sep);
}

void RuntimeStrip::showHelpTooltip(QWidget* anchor, const QString& title, const QString& body)
{
    QString html = QString(
        "<div style='font-family: Segoe UI; font-size: 11px;'>"
        "<b style='color: %1;'>%2</b><br>"
        "<span style='color: %3;'>%4</span>"
        "</div>").arg(Theme::Colors::accent(), title, Theme::Colors::fg(), body);
    QToolTip::showText(anchor->mapToGlobal(QPoint(0, anchor->height() + 4)), html, anchor);
}
