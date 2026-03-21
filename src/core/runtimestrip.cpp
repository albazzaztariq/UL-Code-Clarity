#include "core/runtimestrip.h"
#include "core/theme.h"

#include <QToolTip>
#include <QCursor>

RuntimeStrip::RuntimeStrip(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(26);
    setStyleSheet("background: #2a2a3c; border-bottom: 1px solid #45475a;");

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(12, 0, 12, 0);
    m_layout->setSpacing(12);

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
        setStyleSheet("background: #2a2a3c; border-bottom: 1px solid #45475a;");
    } else {
        setStyleSheet("background: #f0f0f0; border-bottom: 1px solid #d0d0d0;");
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
        dismiss->setStyleSheet(
            "QPushButton { color: #6c7086; background: transparent;"
            " border: none; font-size: 12px; padding: 0; }"
            "QPushButton:hover { color: #cdd6f4; }");
    } else {
        dismiss->setStyleSheet(
            "QPushButton { color: #999999; background: transparent;"
            " border: none; font-size: 12px; padding: 0; }"
            "QPushButton:hover { color: #1e1e2e; }");
    }
    connect(dismiss, &QPushButton::clicked, this, &RuntimeStrip::toggle);
    m_layout->addWidget(dismiss);
}

void RuntimeStrip::addItem(QHBoxLayout* layout, const RuntimeRow& row)
{
    auto* container = new QWidget;
    container->setStyleSheet("background: transparent;");
    auto* itemLayout = new QHBoxLayout(container);
    itemLayout->setContentsMargins(0, 0, 0, 0);
    itemLayout->setSpacing(4);

    // Label
    auto* label = new QLabel(row.label + ":");
    label->setStyleSheet(m_isDark
        ? "QLabel { color: #a6adc8; font-size: 10px; background: transparent; }"
        : "QLabel { color: #555555; font-size: 10px; background: transparent; }");
    itemLayout->addWidget(label);

    if (row.options.isEmpty()) {
        // Static value
        auto* value = new QLabel(row.value);
        value->setStyleSheet(m_isDark
            ? "QLabel { color: #cdd6f4; font-weight: 600; font-size: 10px; background: transparent; }"
            : "QLabel { color: #1e1e2e; font-weight: 600; font-size: 10px; background: transparent; }");
        itemLayout->addWidget(value);
    } else {
        // Tunable combo box
        auto* combo = new QComboBox;
        combo->addItems(row.options);
        if (m_isDark) {
            combo->setStyleSheet(
                "QComboBox { background: #3c3c54; color: #f9e2af;"
                " border: 1px solid #45475a; border-radius: 3px;"
                " padding: 1px 4px; font-size: 10px; font-weight: 600; }"
                "QComboBox::drop-down { border: none; }"
                "QComboBox QAbstractItemView { background: #3c3c54; color: #cdd6f4;"
                " border: 1px solid #45475a; selection-background-color: #45475a; }");
        } else {
            combo->setStyleSheet(
                "QComboBox { background: #ffffff; color: #1e1e2e;"
                " border: 1px solid #d0d0d0; border-radius: 3px;"
                " padding: 1px 4px; font-size: 10px; font-weight: 600; }"
                "QComboBox::drop-down { background: #e8e8e8; border-left: 1px solid #d0d0d0; }"
                "QComboBox QAbstractItemView { background: #ffffff; color: #1e1e2e;"
                " border: 1px solid #d0d0d0; selection-background-color: #d0e0ff;"
                " selection-color: #1e1e2e; }");
        }
        combo->setFixedHeight(18);
        itemLayout->addWidget(combo);
    }

    // Help button (if has help text)
    if (!row.helpTitle.isEmpty()) {
        auto* helpBtn = new QPushButton("?");
        helpBtn->setFixedSize(24, 24);
        helpBtn->setStyleSheet(m_isDark
            ? "QPushButton { background: transparent; color: #a6adc8;"
              " border: none; font-size: 14px; padding: 0; }"
              "QPushButton:hover { color: #cdd6f4; }"
            : "QPushButton { background: transparent; color: #555555;"
              " border: none; font-size: 14px; padding: 0; }"
              "QPushButton:hover { color: #1e1e2e; }");
        QString title = row.helpTitle;
        QString body = row.helpBody;
        connect(helpBtn, &QPushButton::clicked, this, [this, helpBtn, title, body]() {
            showHelpTooltip(helpBtn, title, body);
        });
        itemLayout->addWidget(helpBtn);
    }

    layout->addWidget(container);
}

void RuntimeStrip::addSeparator(QHBoxLayout* layout)
{
    auto* sep = new QLabel("|");
    sep->setStyleSheet(m_isDark
        ? "QLabel { color: #45475a; font-size: 10px; padding: 0 2px; }"
        : "QLabel { color: #cccccc; font-size: 10px; padding: 0 2px; }");
    layout->addWidget(sep);
}

void RuntimeStrip::showHelpTooltip(QWidget* anchor, const QString& title, const QString& body)
{
    QString html = QString(
        "<div style='font-family: Segoe UI; font-size: 11px;'>"
        "<b style='color: #89b4fa;'>%1</b><br>"
        "<span style='color: #cdd6f4;'>%2</span>"
        "</div>").arg(title, body);
    QToolTip::showText(anchor->mapToGlobal(QPoint(0, anchor->height() + 4)), html, anchor);
}
