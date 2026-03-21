#include "panels/langguide.h"
#include <QKeyEvent>
#include <QToolTip>
#include <QMap>
#include <QCursor>
#include <QRegularExpression>

// ============================================================================
// Theme colors
// ============================================================================
static const char* BG      = "#1e1e2e";
static const char* BG3     = "#333348";
static const char* BG4     = "#3c3c54";
static const char* FG      = "#cdd6f4";
static const char* FG2     = "#a6adc8";
static const char* FG3     = "#6c7086";
static const char* ACCENT  = "#89b4fa";
static const char* TEAL    = "#94e2d5";
static const char* GREEN   = "#a6e3a1";
static const char* RED     = "#f38ba8";
static const char* YELLOW  = "#f9e2af";
static const char* BORDER  = "#45475a";

// ============================================================================
// LangGuideOverlay
// ============================================================================

LangGuideOverlay::LangGuideOverlay(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(580);
    setMaximumHeight(600);
    setStyleSheet(QString(
        "LangGuideOverlay { background: %1; border: 1px solid %2; "
        "border-radius: 6px; }"
    ).arg(BG3, BORDER));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(4);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet(QString(
        "font-size: 15px; font-weight: 700; color: %1;"
    ).arg(ACCENT));
    mainLayout->addWidget(m_titleLabel);

    m_subtitleLabel = new QLabel(this);
    m_subtitleLabel->setStyleSheet(QString(
        "font-size: 11px; color: %1; margin-bottom: 16px;"
    ).arg(FG3));
    m_subtitleLabel->setWordWrap(true);
    mainLayout->addWidget(m_subtitleLabel);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 7px; background: transparent; }"
        "QScrollBar::handle:vertical { background: #3c3c54; border-radius: 4px; }"
    );

    m_bodyWidget = new QWidget;
    m_bodyLayout = new QVBoxLayout(m_bodyWidget);
    m_bodyLayout->setContentsMargins(0, 0, 0, 0);
    m_bodyLayout->setSpacing(0);
    m_scrollArea->setWidget(m_bodyWidget);
    mainLayout->addWidget(m_scrollArea, 1);

    m_closeButton = new QPushButton("X", this);
    m_closeButton->setFixedSize(24, 24);
    m_closeButton->setStyleSheet(QString(
        "QPushButton { background: none; color: %1; font-size: 16px; border: none; }"
        "QPushButton:hover { color: %2; }"
    ).arg(FG3, FG));
    m_closeButton->move(width() - 36, 12);
    connect(m_closeButton, &QPushButton::clicked, this, &LangGuideOverlay::closeRequested);

    setVisible(false);
}

void LangGuideOverlay::showForLanguage(const QString &lang)
{
    LangGuideData data;
    if (lang == "python")
        data = pythonGuide();
    else if (lang == "c")
        data = cGuide();
    else if (lang == "ul")
        data = ulGuide();
    else
        return;

    buildGuide(data);
    setVisible(true);
    raise();
}

void LangGuideOverlay::buildGuide(const LangGuideData &data)
{
    m_titleLabel->setText(data.title);
    m_subtitleLabel->setText(data.subtitle);

    // Clear old body
    QLayoutItem *child;
    while ((child = m_bodyLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    for (const auto &section : data.sections) {
        m_bodyLayout->addWidget(createSectionHeader(section.name));
        for (const auto &row : section.rows) {
            m_bodyLayout->addWidget(createRow(row));
        }
    }
    m_bodyLayout->addStretch();
}

QWidget* LangGuideOverlay::createSectionHeader(const QString &name)
{
    auto *label = new QLabel(name);
    label->setStyleSheet(QString(
        "font-size: 11px; font-weight: 700; color: %1; text-transform: uppercase; "
        "letter-spacing: 0.5px; margin: 14px 0 6px; padding-top: 10px; "
        "border-top: 1px solid %2;"
    ).arg(TEAL, BORDER));
    return label;
}

QWidget* LangGuideOverlay::createRow(const LangGuideRow &row)
{
    auto *widget = new QWidget;
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 5, 0, 5);
    layout->setSpacing(8);

    // Label column (160px)
    auto *label = new QLabel(row.label);
    label->setFixedWidth(160);
    label->setStyleSheet(QString("font-size: 12px; color: %1;").arg(FG2));
    layout->addWidget(label);

    // Value column with status color
    auto *value = new QLabel(row.value);
    value->setStyleSheet(QString("font-size: 12px; font-weight: 600; color: %1;").arg(
        row.status == "yes"     ? GREEN :
        row.status == "no"      ? RED :
        row.status == "partial" ? YELLOW : FG
    ));
    layout->addWidget(value, 1);

    // Help button
    if (!row.helpText.isEmpty()) {
        auto *helpBtn = new QPushButton("?", widget);
        helpBtn->setFixedSize(18, 18);
        helpBtn->setCursor(Qt::PointingHandCursor);
        helpBtn->setStyleSheet(QString(
            "QPushButton { background: %1; color: %2; font-size: 9px; "
            "border-radius: 9px; border: none; }"
            "QPushButton:hover { color: %3; background: %4; }"
        ).arg(BG, FG3, ACCENT, BG4));

        QString helpTitle = row.label;
        QString helpBody = row.helpText;
        connect(helpBtn, &QPushButton::clicked, this,
                [this, helpTitle, helpBody, helpBtn]() {
            showHelpTooltip(helpTitle, helpBody, helpBtn);
        });
        layout->addWidget(helpBtn);
    }

    return widget;
}

void LangGuideOverlay::showHelpTooltip(const QString &title, const QString &body,
                                        QWidget *anchor)
{
    QString tooltip = QString("<b style='color:%1'>%2</b><br>%3")
                          .arg(ACCENT, title, body);
    QToolTip::showText(anchor->mapToGlobal(QPoint(0, anchor->height() + 4)),
                       tooltip, anchor);
}

void LangGuideOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit closeRequested();
    }
    QWidget::keyPressEvent(event);
}

// ============================================================================
// Static language guide data -- ported from prototype LANG_TUTORIALS
// ============================================================================

LangGuideData LangGuideOverlay::pythonGuide()
{
    return {
        "Python -- Language Reference",
        "Interpreted, garbage-collected, dynamically typed. Great for beginners and data work. Slow for CPU-heavy tasks.",
        {
            { "Runtime", {
                {"Compiled or Interpreted", "info", "Interpreted (bytecode)", "Python compiles your .py to bytecode (.pyc), then the CPython interpreter reads it instruction by instruction. It never becomes native machine code unless you use tools like Nuitka or Cython."},
                {"Execution Speed", "partial", "Slow for CPU work", "Python is 10-100x slower than C for pure computation. The GIL, dynamic typing, and interpretation all contribute. Fine for I/O, scripting, and glue code."},
                {"Startup Time", "info", "~50ms", "Python loads the interpreter, imports modules, compiles bytecode. Heavier than C but lighter than Java."},
            }},
            { "Memory", {
                {"Memory Model", "info", "Garbage Collected", "Python uses reference counting plus a cyclic garbage collector. You never call malloc or free. Objects are freed when nothing references them."},
                {"Manual Memory Control", "no", "No", "You cannot control when memory is allocated or freed. Python decides. This prevents memory leaks from programmer error but means you can't optimize memory usage."},
                {"Stack vs Heap", "info", "Everything on heap", "All Python objects live on the heap. Local variables are references (pointers) to heap objects. The stack only holds these references, not the data itself."},
            }},
            { "Threading & Concurrency", {
                {"True Parallelism", "no", "No (GIL)", "The Global Interpreter Lock means only one thread executes Python at a time. For CPU parallelism, use multiprocessing (separate processes) or C extensions."},
                {"Async/Await", "yes", "Yes", "Python has native async/await for I/O concurrency. Good for network requests, file I/O, web servers. Not for CPU-bound work."},
                {"Threads", "partial", "Yes, but limited", "threading module exists but threads don't run in parallel for Python code due to the GIL. Useful only for I/O-bound waiting."},
            }},
            { "Object-Oriented Features", {
                {"Classes", "yes", "Yes", "Full class support with constructors (__init__), methods, properties, class variables, static methods."},
                {"Inheritance", "yes", "Yes (multiple)", "Python supports multiple inheritance with MRO (Method Resolution Order). Can inherit from multiple classes."},
                {"Polymorphism", "yes", "Yes (duck typing)", "If it walks like a duck and quacks like a duck, it's a duck. No interfaces needed -- just implement the same methods."},
                {"Interfaces", "partial", "ABC only", "Python has Abstract Base Classes (abc module) but no formal interface keyword. Duck typing is the standard approach."},
                {"Encapsulation", "partial", "Convention only", "Python uses _ prefix for 'private' but nothing is truly private. It's a convention, not enforced by the language."},
            }},
            { "Type System", {
                {"Static Types", "no", "No (optional hints)", "Python is dynamically typed. Type hints (PEP 484) exist but are not enforced at runtime -- they're for tools like mypy."},
                {"Generics", "partial", "Type hints only", "Generic type hints exist (list[int], dict[str, Any]) but are not enforced. The runtime doesn't check them."},
                {"Null Safety", "no", "No", "None can be assigned to anything. No compile-time null checks. NoneType errors are a top Python bug."},
            }},
            { "Quirks & Gotchas", {
                {"Mutable Default Args", "partial", "Dangerous", "def f(items=[]): -- the default list is shared across ALL calls. A classic Python trap. Use None as default instead."},
                {"Indentation is Syntax", "info", "Yes", "Python uses indentation to define blocks. No braces. A misaligned line changes the program's meaning."},
                {"GIL", "partial", "Major limitation", "The Global Interpreter Lock is Python's biggest architectural constraint. It prevents true multi-threaded parallelism for CPU work."},
            }},
        }
    };
}

LangGuideData LangGuideOverlay::cGuide()
{
    return {
        "C -- Language Reference",
        "Compiled to native code. Manual memory. Maximum control, maximum responsibility. The language that built operating systems.",
        {
            { "Runtime", {
                {"Compiled or Interpreted", "info", "Compiled (native binary)", "C compiles directly to machine code. No interpreter, no VM, no runtime. The output is a standalone binary the CPU executes directly."},
                {"Execution Speed", "yes", "Fastest possible", "C code runs at near-hardware speed. No overhead from garbage collection, interpretation, or dynamic dispatch. This is why OS kernels are written in C."},
                {"Startup Time", "yes", "Instant (~1ms)", "A C binary starts immediately. No runtime to load, no bytecode to compile, no modules to import."},
            }},
            { "Memory", {
                {"Memory Model", "info", "Manual (malloc/free)", "You allocate memory with malloc() and free it with free(). Forget to free = memory leak. Free twice = crash. Free then use = undefined behavior."},
                {"Stack vs Heap", "info", "Both, you choose", "Local variables go on the stack (automatic, fast, limited size). Dynamic data goes on the heap (malloc, unlimited, you manage lifetime). Understanding this distinction is critical in C."},
                {"Pointers", "yes", "Yes (central concept)", "Pointers are memory addresses. C gives you direct access to memory through pointers. Powerful but dangerous -- invalid pointers cause segfaults and security vulnerabilities."},
                {"Buffer Overflows", "no", "No protection", "C does not check array bounds. Writing past the end of an array overwrites whatever is next in memory. This is the #1 source of security vulnerabilities in C code."},
            }},
            { "Threading & Concurrency", {
                {"True Parallelism", "yes", "Yes (pthreads)", "C threads run truly in parallel on multiple CPU cores. No GIL. But you must handle synchronization (mutexes, race conditions) yourself."},
                {"Async/Await", "no", "No", "C has no built-in async. You can use libraries like libuv or write event loops manually, but it's not a language feature."},
                {"Data Races", "no", "No protection", "C does nothing to prevent two threads from writing to the same memory simultaneously. You must use mutexes and atomics correctly. Getting this wrong causes hard-to-debug crashes."},
            }},
            { "Object-Oriented Features", {
                {"Classes", "no", "No", "C has no classes. You use structs for data grouping and function pointers for behavior. It's manual but possible."},
                {"Inheritance", "no", "No", "No language support. You can simulate it by embedding one struct inside another, but the compiler doesn't help you."},
                {"Polymorphism", "partial", "Manual (function ptrs)", "You can create vtable-like dispatch using arrays of function pointers, but it's entirely manual. No compiler support."},
                {"Interfaces", "no", "No", "No concept of interfaces. You use conventions and function pointer structs."},
                {"Encapsulation", "partial", "Header files only", "You can hide implementation in .c files and only expose declarations in .h headers. But anything in a struct is accessible."},
            }},
            { "Type System", {
                {"Static Types", "yes", "Yes", "C is statically typed. Every variable has a declared type checked at compile time."},
                {"Generics", "no", "No", "C has no generics. You use void* (type-erased pointers) or preprocessor macros, both losing type safety."},
                {"Null Safety", "no", "No", "NULL can be assigned to any pointer. Dereferencing NULL = crash (segfault). No compile-time protection."},
            }},
            { "Quirks & Gotchas", {
                {"Undefined Behavior", "no", "Everywhere", "C has hundreds of undefined behaviors -- signed overflow, null deref, use-after-free, buffer overflow. The compiler assumes these never happen and optimizes accordingly."},
                {"No Strings", "info", "char arrays + \\0", "C has no string type. Strings are null-terminated char arrays. You manage the memory, the length, and the null terminator yourself."},
                {"Header Files", "info", "Required", "C separates declarations (.h) from implementations (.c). You must manually keep them in sync. Forgetting an include causes cryptic errors."},
                {"Preprocessor", "partial", "Powerful but fragile", "#define macros are text substitution before compilation. They don't respect scope, types, or namespaces. Misuse causes baffling bugs."},
            }},
        }
    };
}

LangGuideData LangGuideOverlay::ulGuide()
{
    return {
        "UniLogic -- Language Reference",
        "Compiled, configurable runtime, multi-target. Python's ease with C's output. Everything is tunable.",
        {
            { "Runtime", {
                {"Compiled or Interpreted", "info", "Compiled (multi-target)", "UL compiles to C (native binary), Python, JavaScript, WASM, or bytecode from the same source. You choose the target."},
                {"Execution Speed", "yes", "C-speed when targeting C", "When compiled to C -> native binary, UL code runs at the same speed as hand-written C. Other targets run at their platform's speed."},
                {"Memory Model", "yes", "You choose", "gc, manual, refcount, or arena -- all from one @dr line. Switch and recompile to see the difference. No other language lets you do this."},
            }},
            { "Memory", {
                {"Garbage Collection", "yes", "Yes (gc mode)", "Using Boehm GC linked into the native binary. Automatic, no free() calls needed."},
                {"Manual Memory", "yes", "Yes (manual mode)", "memtake/memgive (malloc/free). You control allocation and deallocation."},
                {"Reference Counting", "yes", "Yes (refcount mode)", "Automatic increment/decrement. Freed when count hits zero."},
                {"Arena Allocation", "yes", "Yes (arena mode)", "Bulk allocate, free everything at once. Fast for batch processing."},
            }},
            { "Threading & Concurrency", {
                {"True Parallelism", "yes", "Yes (threaded mode)", "pthreads when targeting C. Real OS threads, true parallelism."},
                {"Data Parallelism", "yes", "Yes (parallel mode)", "OpenMP-based parallel for loops. Automatic work distribution."},
                {"Async/Await", "yes", "Yes (async mode)", "Coroutine-based async. Non-blocking I/O."},
                {"Cooperative", "yes", "Yes (cooperative mode)", "Manual yield_now/schedule. Lightweight green threads."},
            }},
            { "Object-Oriented Features", {
                {"Types (structs)", "yes", "Yes", "Zero-overhead C structs. Data only, no methods."},
                {"Objects (OOP)", "yes", "Yes (vtable dispatch)", "Full OOP with constructors, methods, vtable-based polymorphism."},
                {"Inheritance", "yes", "Yes (forks keyword)", "object Dog forks Animal -- copies and extends. Multiple inheritance supported."},
                {"Polymorphism", "yes", "Yes (automatic)", "Variable typed as parent, assigned child -- vtable dispatches correctly."},
                {"Interfaces", "yes", "Yes", "interface keyword. Semcheck enforces all methods present."},
            }},
            { "Type System", {
                {"Static Types", "yes", "Yes (strict mode)", "Full compile-time type checking in strict mode."},
                {"Dynamic Types", "yes", "Yes (dynamic mode)", "Runtime type checking in dynamic mode. Switch with one @dr line."},
                {"Gradual Types", "yes", "Yes (gradual mode)", "Mix typed and untyped. Typed where you want safety, untyped where you want speed."},
                {"Null Safety", "yes", "Yes (@nullable)", "@nullable annotation. Non-nullable by default."},
            }},
        }
    };
}

// ============================================================================
// ExplainHandler
// ============================================================================

ExplainHandler::ExplainHandler(QObject *parent)
    : QObject(parent)
{
    initExplanations();
}

void ExplainHandler::initExplanations()
{
    m_explanations["main.py"] =
        "# function returning float -- calculates total cost of a list of prices with tax applied\n"
        "# accepts: list of prices, optional tax rate (defaults to 10%)\n"
        "def calculate_total(prices, tax_rate=0.10):\n"
        "    \"\"\"Calculate total price with tax.\"\"\"\n"
        "    # guard clause -- returns 0 immediately if the price list is empty, prevents errors downstream\n"
        "    if not prices:\n"
        "        return 0.0\n"
        "\n"
        "    # accumulator variable -- will hold the running sum of all prices\n"
        "    subtotal = 0\n"
        "    # loop through every item in the list by index -- adds each price to the running total\n"
        "    for i in range(len(prices)):\n"
        "        subtotal += prices[i]\n"
        "\n"
        "    # applies tax by multiplying subtotal by (1 + rate), e.g. 1.10 for 10% tax\n"
        "    total = subtotal * (1 + tax_rate)\n"
        "    # rounds to 2 decimal places to avoid floating point artifacts like 50.038999...\n"
        "    return round(total, 2)\n"
        "\n"
        "\n"
        "# entry point -- called when script runs directly, sets up test data and prints results\n"
        "def main():\n"
        "    # test data -- five price values to sum\n"
        "    items = [12.99, 5.50, 3.75, 8.00, 15.25]\n"
        "    # calls calculate_total with default 10% tax\n"
        "    total = calculate_total(items)\n"
        "    # output -- sum() is a builtin that adds list elements, :.2f formats to 2 decimal places\n"
        "    print(f\"Subtotal: ${sum(items):.2f}\")\n"
        "    print(f\"Total with tax: ${total:.2f}\")\n"
        "    # len() returns the number of items in the list\n"
        "    print(f\"Items: {len(items)}\")\n"
        "\n"
        "\n"
        "# Python idiom -- only runs main() if this file is executed directly, not when imported\n"
        "if __name__ == \"__main__\":\n"
        "    main()";

    m_explanations["helpers.c"] =
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "\n"
        "// struct definition -- groups a name string and price float into one data type\n"
        "typedef struct {\n"
        "    char* name;    // pointer to the item's name string\n"
        "    double price;  // the item's price as a 64-bit float\n"
        "} Item;\n"
        "\n"
        "// function returning Item pointer -- allocates heap memory for one Item and sets its fields\n"
        "// caller is responsible for freeing the returned pointer\n"
        "Item* create_item(const char* name, double price) {\n"
        "    Item* item = malloc(sizeof(Item));  // allocates sizeof(Item) bytes on the heap\n"
        "    item->name = (char*)name;           // WARNING: stores pointer directly, doesn't copy the string\n"
        "    item->price = price;\n"
        "    return item;  // returns heap-allocated Item -- must be freed by caller\n"
        "}\n"
        "\n"
        "// function returning double -- sums the price field of each Item in the array\n"
        "// used by main() to get the total cost of all items\n"
        "double total_price(Item** items, int count) {\n"
        "    double total = 0;\n"
        "    for (int i = 0; i < count; i++) {\n"
        "        total += items[i]->price;  // arrow operator dereferences pointer to access price field\n"
        "    }\n"
        "    return total;\n"
        "}\n"
        "\n"
        "// entry point -- creates 3 items, prints their total, but never frees the allocated memory\n"
        "int main() {\n"
        "    Item* items[3];  // array of 3 pointers to Item structs (stack-allocated array of heap pointers)\n"
        "    items[0] = create_item(\"Widget\", 12.99);\n"
        "    items[1] = create_item(\"Gadget\", 5.50);\n"
        "    items[2] = create_item(\"Doohickey\", 3.75);\n"
        "\n"
        "    // %.2f formats the double to 2 decimal places\n"
        "    printf(\"Total: $%.2f\\n\", total_price(items, 3));\n"
        "\n"
        "    // BUG: memory leak -- 3 malloc'd Items are never free()'d before program exits\n"
        "    return 0;\n"
        "}";

    m_explanations["app.ul"] =
        "// function returning float -- sums a list of prices and applies a tax multiplier\n"
        "// called by main() to compute the final cost\n"
        "function calculate_total(list float prices, float tax_rate) returns float\n"
        "    // guard clause -- returns 0 if the list is empty, avoids iterating over nothing\n"
        "    if size(prices) == 0\n"
        "        return 0.0\n"
        "    end if\n"
        "\n"
        "    // accumulator -- holds running sum, initialized to 0.0 (float)\n"
        "    float subtotal = 0.0\n"
        "    // for-each loop -- iterates every element in the prices list, adds to subtotal\n"
        "    for each p in prices\n"
        "        subtotal += p\n"
        "    end for\n"
        "\n"
        "    // multiplies subtotal by (1 + tax_rate) and returns the taxed total\n"
        "    return subtotal * (1.0 + tax_rate)\n"
        "end function\n"
        "\n"
        "// entry point -- sets up test data and prints the result\n"
        "// returns int (0 = success) as required by UL convention\n"
        "function main() returns int\n"
        "    // list literal -- 5 float prices\n"
        "    list float items = [12.99, 5.50, 3.75, 8.00, 15.25]\n"
        "    // calls calculate_total with 10% tax rate\n"
        "    float total = calculate_total(items, 0.10)\n"
        "    // change() casts float to string for concatenation -- UL has no string interpolation\n"
        "    print \"Total with tax: \" + change(total)->string\n"
        "    return 0\n"
        "end function";
}

QString ExplainHandler::getExplainedCode(const QString &filename) const
{
    return m_explanations.value(filename);
}

bool ExplainHandler::hasExplanation(const QString &filename) const
{
    return m_explanations.contains(filename);
}

// ── Local line-by-line annotator ─────────────────────────────────────────────

// Classify a single (trimmed) line and return a short explanation string.
// Returns empty string if the line needs no comment (blank, already a comment, closing brace).
static QString classifyLine(const QString &trimmed, const QString &lang, int level)
{
    if (trimmed.isEmpty()) return {};

    bool isPy = (lang == "python" || lang == "ul");
    bool isC  = (lang == "c" || lang == "cpp" || lang == "rust" || lang == "js");

    // Already a comment — skip
    if (trimmed.startsWith("//") || trimmed.startsWith("/*") || trimmed.startsWith("*"))
        return {};
    if (isPy && trimmed.startsWith("#")) return {};

    // Single closing brace/bracket — skip at L3+
    if ((trimmed == "}" || trimmed == "{" || trimmed == "};") && level >= 3) return {};

    // ── Imports / includes ────────────────────────────────────────────────
    {
        QRegularExpression pyImport(R"(^(?:import|from)\s+(\S+))");
        auto m = pyImport.match(trimmed);
        if (isPy && m.hasMatch()) {
            QString mod = m.captured(1);
            if (level == 1)
                return QString("imports the \"%1\" module so we can use its functions").arg(mod);
            if (level == 2)
                return QString("import: %1").arg(mod);
            return {};  // L3/L4: skip import comments
        }
    }
    {
        QRegularExpression inc(R"(^#include\s*[<"]([\w./]+)[>"])");
        auto m = inc.match(trimmed);
        if (isC && m.hasMatch()) {
            QString hdr = m.captured(1);
            if (level == 1)
                return QString("includes the \"%1\" header to access its functions").arg(hdr);
            if (level == 2)
                return QString("include: %1").arg(hdr);
            return {};
        }
    }

    // ── Function definitions ──────────────────────────────────────────────
    {
        // Python/UL: def funcname(params):
        QRegularExpression pyDef(R"(^(?:def|fn|func)\s+(\w+)\s*\(([^)]*)\))");
        auto m = pyDef.match(trimmed);
        if (isPy && m.hasMatch()) {
            QString name = m.captured(1);
            QString params = m.captured(2).trimmed();
            if (level == 1) {
                if (params.isEmpty())
                    return QString("defines a function called \"%1\" that takes no inputs").arg(name);
                return QString("defines a function called \"%1\" — inputs: %2").arg(name, params);
            }
            if (level == 2)
                return QString("function %1(%2)").arg(name, params);
            if (level == 3)
                return QString("fn %1 | params: %2").arg(name, params.isEmpty() ? "none" : params);
            return {};  // L4: no comment on fn definition
        }
    }
    {
        // C/C++: return_type funcname( at start of line (no semicolon = definition)
        QRegularExpression cDef(R"(^\s*(?:static\s+|inline\s+|extern\s+)?(?:[\w:*&<>\[\]]+\s+)+(\w+)\s*\([^;]*$)");
        auto m = cDef.match(trimmed);
        if (isC && m.hasMatch() && !trimmed.endsWith(';')) {
            QString name = m.captured(1);
            static QStringList kw = {"if","for","while","switch","return","else","do","case"};
            if (!kw.contains(name)) {
                if (level == 1) return QString("defines the \"%1\" function").arg(name);
                if (level == 2) return QString("function: %1").arg(name);
                if (level == 3) return QString("fn %1").arg(name);
                return {};
            }
        }
    }

    // ── Class / struct definitions ────────────────────────────────────────
    {
        QRegularExpression classDef(R"(^(?:class|struct|enum)\s+(\w+))");
        auto m = classDef.match(trimmed);
        if (m.hasMatch()) {
            QString name = m.captured(1);
            QString kind = trimmed.startsWith("class") ? "class" :
                           trimmed.startsWith("struct") ? "struct" : "enum";
            if (level == 1)
                return QString("defines a %1 called \"%2\" — a blueprint for creating objects").arg(kind, name);
            if (level == 2)
                return QString("%1 definition: %2").arg(kind, name);
            return {};
        }
    }

    // ── Loops ─────────────────────────────────────────────────────────────
    {
        QRegularExpression forLoop(R"(^for\s+(\w+)\s+in\s+(.+):?\s*$)");
        auto m = forLoop.match(trimmed);
        if (isPy && m.hasMatch()) {
            QString var = m.captured(1);
            QString iterable = m.captured(2).trimmed().remove(':');
            if (level == 1)
                return QString("loop: for each \"%1\" in \"%2\", run the indented block below").arg(var, iterable);
            if (level == 2)
                return QString("iterate %1 over %2").arg(var, iterable);
            return {};
        }
    }
    {
        QRegularExpression forC(R"(^for\s*\()");
        if (isC && forC.match(trimmed).hasMatch()) {
            if (level == 1) return "loop: repeats the block below, counting with the loop variable";
            if (level == 2) return "for loop";
            return {};
        }
    }
    {
        QRegularExpression whileRx(R"(^while\s*[\(])");
        if (isC && whileRx.match(trimmed).hasMatch()) {
            if (level == 1) return "loop: keeps repeating while the condition inside ( ) is true";
            if (level == 2) return "while loop";
            return {};
        }
        QRegularExpression whilePy(R"(^while\s+.+:)");
        if (isPy && whilePy.match(trimmed).hasMatch()) {
            if (level == 1) return "loop: repeats while the condition is true";
            if (level == 2) return "while loop";
            return {};
        }
    }

    // ── Conditionals ──────────────────────────────────────────────────────
    {
        QRegularExpression ifRx(R"(^if\s+(.+?)(?:\s*:|\s*\{)?\s*$)");
        auto m = ifRx.match(trimmed);
        if (m.hasMatch()) {
            QString cond = m.captured(1).trimmed().remove(':').remove('{');
            if (level == 1)
                return QString("checks: if \"%1\" is true, run the block below").arg(cond);
            if (level == 2)
                return QString("condition: %1").arg(cond);
            return {};
        }
    }
    if (trimmed.startsWith("elif ") || trimmed.startsWith("else if")) {
        if (level == 1) return "otherwise, checks a different condition";
        if (level == 2) return "else if branch";
        return {};
    }
    if (trimmed == "else:" || trimmed == "else {" || trimmed == "else") {
        if (level == 1) return "if none of the above conditions matched, run this block";
        if (level == 2) return "fallback else branch";
        return {};
    }

    // ── Return statements ─────────────────────────────────────────────────
    {
        QRegularExpression retRx(R"(^return\s*(.*))");
        auto m = retRx.match(trimmed);
        if (m.hasMatch()) {
            QString val = m.captured(1).trimmed().remove(';');
            if (level == 1) {
                if (val.isEmpty()) return "exits the function, returning nothing";
                return QString("exits the function and sends back \"%1\"").arg(val);
            }
            if (level == 2)
                return QString("return %1").arg(val.isEmpty() ? "void" : val);
            return {};
        }
    }

    // ── Print / output statements ─────────────────────────────────────────
    {
        QRegularExpression printRx(R"(^(?:print|printf|println|console\.log|puts|cout)\b)");
        if (printRx.match(trimmed).hasMatch()) {
            if (level == 1) return "prints output to the screen for the user to see";
            if (level == 2) return "output to console";
            return {};
        }
    }

    // ── Variable declarations / assignments ───────────────────────────────
    {
        // Python: name = value  or  name: type = value
        QRegularExpression pyAssign(R"(^(\w+)(?:\s*:\s*\w+)?\s*=\s*(.+))");
        auto m = pyAssign.match(trimmed);
        if (isPy && m.hasMatch() && !trimmed.contains("==") && !trimmed.startsWith("if")) {
            QString name = m.captured(1);
            QString val  = m.captured(2).trimmed();
            if (level == 1)
                return QString("creates a variable called \"%1\" and sets it to %2").arg(name, val);
            if (level == 2)
                return QString("%1 = %2").arg(name, val);
            return {};
        }
    }
    {
        // C/C++: type name = val;  or  auto name = val;
        QRegularExpression cDecl(R"(^(?:int|float|double|char|bool|auto|long|short|unsigned|const|string|QString|var|let|const)\s+(\w+)\s*(?:=\s*(.+?))?;)");
        auto m = cDecl.match(trimmed);
        if (isC && m.hasMatch()) {
            QString name = m.captured(1);
            QString val  = m.captured(2).trimmed();
            if (level == 1) {
                if (val.isEmpty())
                    return QString("declares a variable called \"%1\"").arg(name);
                return QString("declares \"%1\" and sets it to %2").arg(name, val);
            }
            if (level == 2)
                return QString("var %1%2").arg(name, val.isEmpty() ? "" : " = " + val);
            return {};
        }
    }

    // ── Function calls (catch-all for lines ending in ;  or  ) ) ──────────
    {
        QRegularExpression callRx(R"(^(\w+)\s*\()");
        auto m = callRx.match(trimmed);
        if (m.hasMatch()) {
            QString fn = m.captured(1);
            static QStringList skip = {"if","for","while","switch","return","def","fn","func","class","struct"};
            if (!skip.contains(fn)) {
                if (level == 1)
                    return QString("calls the \"%1\" function").arg(fn);
                if (level == 2)
                    return QString("call: %1()").arg(fn);
                return {};
            }
        }
    }

    return {};  // no annotation for this line
}

QString ExplainHandler::generateExplainedCode(const QString &code,
                                               const QString &lang,
                                               int level)
{
    if (code.trimmed().isEmpty()) return code;

    // L4 (No Assist): only annotate function signatures, nothing else
    // L1-L3: annotate lines based on classification

    bool isPy = (lang == "python" || lang == "ul");
    QString commentPrefix = isPy ? "# " : "// ";

    QStringList lines = code.split('\n');
    QStringList result;

    // Header comment
    QString levelName;
    if (level == 1) levelName = "Beginner";
    else if (level == 2) levelName = "Intermediate";
    else if (level == 3) levelName = "Developer";
    else levelName = "Signature-only";

    result << commentPrefix + QString("── Explained by Code Clarity (Level %1: %2) ──")
                                  .arg(level).arg(levelName);
    result << "";

    for (const QString &rawLine : lines) {
        QString trimmed = rawLine.trimmed();
        QString explanation = classifyLine(trimmed, lang, level);

        if (!explanation.isEmpty()) {
            // Preserve leading indentation for the comment
            QString indent;
            for (const QChar &ch : rawLine) {
                if (ch == ' ' || ch == '\t') indent += ch;
                else break;
            }
            result << indent + commentPrefix + explanation;
        }
        result << rawLine;
    }

    return result.join('\n');
}
