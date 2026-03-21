#include "panels/basics.h"
#include <QKeyEvent>
#include <QScrollBar>

// ============================================================================
// Theme colors
// ============================================================================
static const char* BG      = "#1e1e2e";
static const char* BG2     = "#2a2a3c";
static const char* BG3     = "#333348";
static const char* BG4     = "#3c3c54";
static const char* FG      = "#cdd6f4";
static const char* FG2     = "#a6adc8";
static const char* FG3     = "#6c7086";
static const char* ACCENT  = "#89b4fa";
static const char* GREEN   = "#a6e3a1";
static const char* RED     = "#f38ba8";
static const char* YELLOW  = "#f9e2af";
static const char* BORDER  = "#45475a";

// ============================================================================
// BasicsOverlay
// ============================================================================

BasicsOverlay::BasicsOverlay(QWidget *parent)
    : QWidget(parent), m_currentPage(0)
{
    initPages();
    buildUI();
    setVisible(false);
}

void BasicsOverlay::initPages()
{
    // All 12 pages ported from prototype BASICS_PAGES
    m_pages = {
        { "What is Code?", "Page 1 of 12", {
            { "Code is Instructions", "Code is a list of instructions that tells a computer what to do, step by step. Like a recipe -- the computer follows it exactly." },
            { "Why Text?", "You could click buttons in a visual tool, but text is faster, more precise, and what every professional uses. Every app on your phone was written as text." },
            { "Languages", "Just like human languages, there are many programming languages. Each has strengths. Python is easy to read. C is fast. JavaScript runs in browsers. You pick the one that fits your job." },
        }},
        { "Variables and Types", "Page 2 of 12", {
            { "Variables", "A variable is a named container. <code>price = 12.99</code> stores the number 12.99 under the name \"price\". You can read it, change it, and use it later." },
            { "Types", "Types describe what kind of data something is. <code>42</code> is an integer (whole number). <code>3.14</code> is a float (decimal). <code>\"hello\"</code> is a string (text). <code>true</code> is a boolean (yes/no)." },
            { "Why Types Matter", "You can add two numbers: <code>5 + 3 = 8</code>. You can add two strings: <code>\"hi\" + \" there\" = \"hi there\"</code>. But <code>5 + \"hi\"</code> doesn't make sense -- the type tells the computer what \"add\" means." },
        }},
        { "Functions and Methods", "Page 3 of 12", {
            { "Functions", "A function is a reusable block of code with a name. <code>calculate_total(prices)</code> takes a list of prices and returns the total. You write it once, call it as many times as you want." },
            { "Parameters and Returns", "Parameters are the inputs (what you pass in). Return values are the outputs (what comes back). <code>add(3, 7)</code> passes 3 and 7 in, gets 10 back." },
            { "Methods", "A method is a function that belongs to an object. <code>myList.append(5)</code> -- append is a method on the list. Same idea as a function, just attached to something." },
        }},
        { "Loops and Conditions", "Page 4 of 12", {
            { "If/Else", "Decisions. <code>if temperature > 100: alarm()</code> -- the code only runs when the condition is true. <code>else</code> handles the other case." },
            { "Loops", "Repeating things. <code>for item in list: process(item)</code> -- runs the body once for every item. Without loops, you'd copy-paste the same code hundreds of times." },
            { "While Loops", "<code>while not done: keep_trying()</code> -- repeats until a condition changes. Useful when you don't know how many times you'll need to loop." },
        }},
        { "Objects, Classes, and Structs", "Page 5 of 12", {
            { "Structs / Types", "A struct groups related data together. An <code>Item</code> struct might have a <code>name</code> (string) and <code>price</code> (float). Instead of two separate variables, you have one thing with two fields." },
            { "Classes and Objects", "A class is a struct that also has functions (methods) attached. An object is one instance of a class. <code>class Dog</code> defines the template. <code>myDog = Dog(\"Rex\")</code> creates one specific dog." },
            { "Inheritance", "A class can extend another. <code>class Poodle extends Dog</code> -- Poodle gets everything Dog has, plus its own additions. Not all languages have this -- C doesn't. Python and UL do." },
            { "Polymorphism", "Using a parent type to handle different children. A variable typed as <code>Animal</code> can hold a Dog, Cat, or Bird. The right method runs based on what's actually stored. This is how plugins and frameworks work." },
        }},
        { "Algorithms", "Page 6 of 12", {
            { "What's an Algorithm?", "A step-by-step recipe for solving a problem. \"Sort this list from smallest to largest\" is a problem. Bubble sort, merge sort, quicksort are algorithms -- different recipes for the same result, with different speeds." },
            { "Big O Notation", "A way to describe how slow something gets as the input grows. O(n) means the time grows linearly -- twice the data, twice the time. O(n^2) means it gets much worse. You don't need to memorize this, but knowing it exists helps you understand why some code is slow." },
            { "You Already Use Them", "Every time you search, sort, filter, or deduplicate data -- that's an algorithm. Libraries like Python's <code>sorted()</code> use optimized algorithms so you don't have to write them yourself." },
        }},
        { "How Code Runs", "Page 7 of 12", {
            { "The Call Stack", "When a function calls another function, the computer remembers where it was. This stack of \"return addresses\" is the call stack. When the inner function finishes, execution returns to where it left off. Stack overflow = too many nested calls." },
            { "Compiled vs Interpreted", "<b>Compiled</b> (C, Rust): your code is translated to machine code BEFORE it runs. The output is a standalone binary. <b>Interpreted</b> (Python): your code is read and executed line by line by another program (the interpreter)." },
            { "JIT vs AOT", "<b>AOT</b> (Ahead of Time): compile everything before running. C does this. <b>JIT</b> (Just in Time): compile hot paths while running. Java and JavaScript do this. JIT can be fast but uses more memory at startup." },
            { "VMs and Bytecode", "Some languages (Java, Python) compile to an intermediate format (bytecode), then a virtual machine interprets it. The VM is a program that pretends to be a computer. Slower than native code, but portable -- runs anywhere the VM exists." },
        }},
        { "Memory", "Page 8 of 12", {
            { "What is Memory?", "RAM -- temporary, fast storage. Every variable, object, and string lives in memory while your program runs. When the program exits, it's all gone." },
            { "Stack vs Heap", "The <b>stack</b> is small, fast, automatic -- local variables go here. The <b>heap</b> is large, slower, manual -- objects you create with <code>new</code> or <code>malloc</code> go here. Python puts everything on the heap. C lets you choose." },
            { "Garbage Collection", "Some languages (Python, Java, Go) automatically find and free memory you're no longer using. You never think about it. The downside: the GC pauses your program briefly to do its work." },
            { "Manual Memory", "In C, YOU allocate (<code>malloc</code>) and free (<code>free</code>). Forget to free = memory leak (your program slowly eats all RAM). Free twice = crash. Use after free = security vulnerability." },
            { "Memory Safety", "The #1 source of security bugs is memory errors in C/C++. Rust solves this with ownership rules. UL lets you pick your model. Python avoids it entirely with GC." },
        }},
        { "Platforms and Compilation", "Page 9 of 12", {
            { "What is a Platform?", "A platform is an OS + CPU architecture combination. <b>Windows x64</b>, <b>macOS ARM64</b>, <b>Linux x64</b> are platforms. Code compiled for one platform doesn't run on another -- the machine instructions are different." },
            { "ISA (Instruction Set Architecture)", "The CPU's language. x64 (Intel/AMD) and ARM64 (Apple Silicon, phones) speak different instruction sets. A binary compiled for x64 is gibberish to an ARM chip. This is why you download different versions of software for different machines." },
            { "Why C/C++ Must Target Each Platform", "C compiles to native machine code for a specific ISA. To support Windows, Mac, and Linux, you compile three separate times. Python avoids this -- the interpreter handles platform differences. The trade-off: Python is slower." },
            { "LLVM and IR", "LLVM is a compiler toolkit. It takes an intermediate representation (IR) -- a platform-neutral format -- and compiles it to any ISA. Write one compiler front-end, get every platform for free. Rust, Swift, and UL use LLVM." },
            { "Assembly", "The human-readable form of machine code. <code>MOV RAX, 5</code> means \"put 5 in register RAX\". You almost never write this directly, but debuggers show it when things go wrong at the lowest level." },
        }},
        { "Version Control", "Page 10 of 12", {
            { "Git", "A system that tracks every change you make to your code. You can go back to any previous version, see who changed what, and work on multiple features simultaneously without breaking each other's work." },
            { "Commit", "A snapshot of your code at a point in time with a message describing what changed. <code>git commit -m \"fix login bug\"</code>. Your project history is a chain of commits." },
            { "Push and Pull", "<b>Push</b>: upload your commits to a remote server (GitHub). <b>Pull</b>: download other people's commits. This is how teams collaborate -- everyone pushes and pulls from the same repository." },
            { "GitHub", "A website that hosts Git repositories. It adds code review (pull requests), issue tracking, CI/CD (automatic testing), and collaboration features. Most open-source software lives on GitHub." },
            { "Branches", "A branch is a parallel timeline. You create a branch to work on a feature without touching the main code. When it's ready, you merge it back. If it doesn't work out, you delete the branch -- main is untouched." },
        }},
        { "Languages at a Glance", "Page 11 of 12", {
            { "Python", "Easy to read, huge ecosystem, slow for CPU work. Used for: AI/ML, data science, scripting, web backends, automation. The most popular first language." },
            { "JavaScript", "The language of the web browser. Every website uses it. Also runs on servers (Node.js). Dynamic, flexible, quirky. Used for: websites, web apps, mobile (React Native), servers." },
            { "C", "The foundation. Operating systems, databases, embedded devices. Fast as possible, zero overhead. Manual memory. Dangerous if you're not careful. 50+ years old and still everywhere." },
            { "C++", "C with classes, templates, and RAII. Used for: game engines (Unreal), browsers (Chrome), databases, performance-critical systems. Complex but powerful." },
            { "Rust", "Memory safety without garbage collection. The compiler enforces ownership rules -- if it compiles, it's safe. Used for: systems programming, CLI tools, WebAssembly. Steep learning curve." },
            { "Java", "Enterprise workhorse. Runs on the JVM -- write once, run anywhere. Verbose but reliable. Used for: Android apps, enterprise backends, big data (Hadoop/Spark)." },
            { "Go", "Google's language for servers. Simple, fast compilation, great concurrency. Used for: cloud infrastructure (Docker, Kubernetes), microservices, CLIs." },
            { "UniLogic", "Write once, compile to C, Python, JS, WASM, or bytecode. Configurable memory and concurrency. Designed for people who want to understand what's happening under the hood." },
        }},
        { "What Else You Should Know", "Page 12 of 12", {
            { "APIs", "An API (Application Programming Interface) is how programs talk to each other. When your app fetches weather data, it calls a weather API -- sending a request and getting structured data back." },
            { "Debugging", "Finding and fixing bugs. Read the error message. Check the line number. Add print statements. Use a debugger to step through line by line. 90% of debugging is reading carefully." },
            { "Testing", "Writing code that checks your other code. <code>assert add(2,3) == 5</code>. If someone breaks the add function later, the test catches it immediately. Professional code has thousands of tests." },
            { "Dependencies", "Libraries other people wrote that your code uses. <code>pip install requests</code> gives you HTTP in Python. The risk: dependencies can break, get abandoned, or have security holes. Keep them updated." },
            { "The Terminal", "A text interface to your computer. You type commands, it executes them. Faster than clicking through menus once you learn it. Every developer uses it daily." },
        }},
    };
}

void BasicsOverlay::buildUI()
{
    setStyleSheet(QString("background: %1;").arg(BG));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Close button (top-right)
    m_closeButton = new QPushButton("X Close", this);
    m_closeButton->setStyleSheet(QString(
        "QPushButton { background: %1; color: #1e1e2e; font-weight: 700; "
        "padding: 6px 14px; border-radius: 6px; font-size: 12px; }"
    ).arg(RED));
    m_closeButton->setFixedHeight(28);
    connect(m_closeButton, &QPushButton::clicked, this, &BasicsOverlay::closeRequested);

    auto *closeRow = new QHBoxLayout;
    closeRow->addStretch();
    closeRow->addWidget(m_closeButton);
    closeRow->setContentsMargins(0, 12, 20, 0);
    mainLayout->addLayout(closeRow);

    // Scroll area for page content
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 7px; background: transparent; }"
        "QScrollBar::handle:vertical { background: #3c3c54; border-radius: 4px; }"
    );

    m_contentWidget = new QWidget;
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(0, 0, 0, 60); // space for nav bar
    m_contentLayout->setAlignment(Qt::AlignHCenter);

    // Build all page widgets
    for (int i = 0; i < m_pages.size(); i++) {
        auto *pageWidget = createPageWidget(m_pages[i]);
        pageWidget->setVisible(i == 0);
        m_contentLayout->addWidget(pageWidget);
        m_pageWidgets.append(pageWidget);
    }

    m_scrollArea->setWidget(m_contentWidget);
    mainLayout->addWidget(m_scrollArea, 1);

    // Navigation bar at the bottom
    m_navBar = new QWidget(this);
    m_navBar->setFixedHeight(50);
    m_navBar->setStyleSheet(QString(
        "background: %1; border-top: 1px solid %2;"
    ).arg(BG2, BORDER));

    auto *navLayout = new QHBoxLayout(m_navBar);
    navLayout->setContentsMargins(24, 10, 24, 10);

    m_prevButton = new QPushButton("<- Back", m_navBar);
    m_prevButton->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; padding: 6px 20px; "
        "font-size: 12px; border-radius: 6px; }"
    ).arg(BG3, FG));
    connect(m_prevButton, &QPushButton::clicked, this, &BasicsOverlay::prevPage);
    navLayout->addWidget(m_prevButton);

    navLayout->addStretch();

    // Dots row
    auto *dotsWidget = new QWidget(m_navBar);
    m_dotsLayout = new QHBoxLayout(dotsWidget);
    m_dotsLayout->setContentsMargins(0, 0, 0, 0);
    m_dotsLayout->setSpacing(4);
    for (int i = 0; i < m_pages.size(); i++) {
        auto *dot = new QPushButton(m_navBar);
        dot->setFixedSize(8, 8);
        dot->setCursor(Qt::PointingHandCursor);
        int pageIndex = i;
        connect(dot, &QPushButton::clicked, this, [this, pageIndex]() {
            goToPage(pageIndex);
        });
        m_dotsLayout->addWidget(dot);
        m_dots.append(dot);
    }
    navLayout->addWidget(dotsWidget);

    navLayout->addStretch();

    m_nextButton = new QPushButton("Next ->", m_navBar);
    m_nextButton->setStyleSheet(QString(
        "QPushButton { background: %1; color: #1e1e2e; padding: 6px 20px; "
        "font-size: 12px; border-radius: 6px; }"
    ).arg(ACCENT));
    connect(m_nextButton, &QPushButton::clicked, this, &BasicsOverlay::nextPage);
    navLayout->addWidget(m_nextButton);

    mainLayout->addWidget(m_navBar);

    updateNavigation();
}

QWidget* BasicsOverlay::createPageWidget(const BasicsPage &page)
{
    auto *widget = new QWidget;
    widget->setMaximumWidth(640);

    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(24, 32, 24, 24);
    layout->setSpacing(10);

    auto *title = new QLabel(page.title, widget);
    title->setStyleSheet(QString(
        "font-size: 22px; font-weight: 700; color: %1;"
    ).arg(ACCENT));
    layout->addWidget(title);

    auto *sub = new QLabel(page.subtitle, widget);
    sub->setStyleSheet(QString("font-size: 12px; color: %1; margin-bottom: 20px;").arg(FG3));
    layout->addWidget(sub);

    for (const auto &card : page.cards) {
        layout->addWidget(createCardWidget(card));
    }

    layout->addStretch();
    return widget;
}

QWidget* BasicsOverlay::createCardWidget(const BasicsCard &card)
{
    auto *frame = new QFrame;
    frame->setStyleSheet(QString(
        "QFrame { background: %1; border-radius: 6px; border-left: 3px solid %2; }"
    ).arg(BG2, ACCENT));

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(4);

    auto *heading = new QLabel(card.heading, frame);
    heading->setStyleSheet(QString("font-size: 13px; font-weight: 700; color: %1;").arg(FG));
    layout->addWidget(heading);

    auto *body = new QLabel(card.body, frame);
    body->setStyleSheet(QString("font-size: 12px; color: %1; line-height: 1.55;").arg(FG2));
    body->setWordWrap(true);
    body->setTextFormat(Qt::RichText);
    layout->addWidget(body);

    return frame;
}

void BasicsOverlay::goToPage(int index)
{
    if (index < 0 || index >= m_pages.size()) return;

    m_currentPage = index;

    for (int i = 0; i < m_pageWidgets.size(); i++) {
        m_pageWidgets[i]->setVisible(i == index);
    }

    // Scroll to top
    m_scrollArea->verticalScrollBar()->setValue(0);

    updateNavigation();
}

void BasicsOverlay::nextPage()
{
    if (m_currentPage == m_pages.size() - 1) {
        emit closeRequested();
    } else {
        goToPage(m_currentPage + 1);
    }
}

void BasicsOverlay::prevPage()
{
    goToPage(m_currentPage - 1);
}

int BasicsOverlay::currentPage() const
{
    return m_currentPage;
}

int BasicsOverlay::pageCount() const
{
    return m_pages.size();
}

void BasicsOverlay::updateNavigation()
{
    // Update dots
    for (int i = 0; i < m_dots.size(); i++) {
        QString color;
        if (i == m_currentPage)
            color = ACCENT;
        else if (i < m_currentPage)
            color = FG3;
        else
            color = BG4;

        m_dots[i]->setStyleSheet(QString(
            "QPushButton { background: %1; border-radius: 4px; border: none; }"
        ).arg(color));
    }

    // Update prev button visibility
    m_prevButton->setVisible(m_currentPage > 0);

    // Update next button text
    if (m_currentPage == m_pages.size() - 1) {
        m_nextButton->setText("Done");
        m_nextButton->setStyleSheet(QString(
            "QPushButton { background: %1; color: #1e1e2e; padding: 6px 20px; "
            "font-size: 12px; border-radius: 6px; }"
        ).arg(GREEN));
    } else {
        m_nextButton->setText("Next ->");
        m_nextButton->setStyleSheet(QString(
            "QPushButton { background: %1; color: #1e1e2e; padding: 6px 20px; "
            "font-size: 12px; border-radius: 6px; }"
        ).arg(ACCENT));
    }
}

void BasicsOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit closeRequested();
    } else if (event->key() == Qt::Key_Right) {
        nextPage();
    } else if (event->key() == Qt::Key_Left) {
        prevPage();
    }
    QWidget::keyPressEvent(event);
}

// ============================================================================
// BuildFromScratchMode
// ============================================================================

BuildFromScratchMode::BuildFromScratchMode(QObject *parent)
    : QObject(parent), m_active(false), m_phase(Design)
{
}

bool BuildFromScratchMode::isActive() const
{
    return m_active;
}

BuildFromScratchMode::Phase BuildFromScratchMode::currentPhase() const
{
    return m_phase;
}

QString BuildFromScratchMode::phaseLabel() const
{
    switch (m_phase) {
    case Design:             return "Phase: Design";
    case LanguageSelection:  return "Phase: Language Selection";
    case Packages:           return "Phase: Packages";
    case Architecture:       return "Phase: Architecture";
    case Building:           return "Phase: Building";
    }
    return "Phase: Unknown";
}

void BuildFromScratchMode::activate()
{
    m_active = true;
    m_phase = Design;
    emit activated();
    emit showEditorPrompt(true);
}

void BuildFromScratchMode::deactivate()
{
    m_active = false;
    m_phase = Design;
    emit deactivated();
    emit showEditorPrompt(false);
}

void BuildFromScratchMode::advancePhase()
{
    if (m_phase < Building) {
        m_phase = static_cast<Phase>(static_cast<int>(m_phase) + 1);
        emit phaseChanged(m_phase);
    }
}

QString BuildFromScratchMode::generateResponse(const QString &userInput)
{
    QString response;

    switch (m_phase) {
    case Design:
        m_projectDescription = userInput;
        response = QString(
            "Great -- so you want to build <b>%1</b>.<br><br>"
            "Let me think about the best approach:<br><br>"
            "<b>Language:</b> I'd recommend <b>Python</b> for this. Here's why:<br>"
            "* Fast to prototype -- you'll see results quickly<br>"
            "* Great libraries for what you're describing<br>"
            "* Runs on both Windows and Mac without changes<br><br>"
            "If you wanted raw speed, C would work but you'd spend 3x longer on memory "
            "management. Rust would be safe but the learning curve is steep for a first project.<br><br>"
            "<b>Does Python sound good? Or would you prefer another language?</b>"
        ).arg(userInput.left(60));
        advancePhase();
        break;

    case LanguageSelection:
        response =
            "Python it is. Now let me figure out what packages we'll need:<br><br>"
            "<b>Packages:</b><br>"
            "* <code>json</code> -- built-in, for saving/loading data to files<br>"
            "* <code>os</code> -- built-in, for file path handling<br>"
            "* <code>datetime</code> -- built-in, for timestamps<br><br>"
            "No external installs needed -- everything's included with Python.<br><br>"
            "<b>Does this look right? Any features I'm missing that might need other libraries?</b>";
        advancePhase();
        break;

    case Packages:
        response =
            "Now let's design the structure. Based on what you described, here's what we need:<br><br>"
            "<b>Functions:</b><br>"
            "* <code>add_item(name, description)</code> -- creates a new entry<br>"
            "* <code>remove_item(id)</code> -- deletes by ID<br>"
            "* <code>list_items()</code> -- shows everything<br>"
            "* <code>save_to_file(path)</code> -- writes data to disk<br>"
            "* <code>load_from_file(path)</code> -- reads data from disk<br><br>"
            "<b>Data:</b> A list of dicts, each with id, name, description, created_at, done<br><br>"
            "<b>Flow:</b> main() loads the file -> shows a menu -> user picks action -> "
            "loops until quit -> saves on exit<br><br>"
            "<b>Does this design make sense? Want to add or change anything before we start coding?</b>";
        advancePhase();
        break;

    case Architecture:
        response =
            "Let's start writing code. I'll create the first function and explain each line as we go.<br><br>"
            "<b>Ready? I'll begin with the data structure and add_item().</b><br>"
            "You'll write parts of it yourself -- I'll guide you through what each line does.";
        advancePhase();
        emit showEditorPrompt(false);
        emit scaffoldCode(QString(
            "# -- Your project: %1 --\n"
            "# We'll build this together, one function at a time.\n"
            "\n"
            "# Step 1: Define the data structure\n"
            "# A list that holds all our items\n"
            "items = []\n"
            "\n"
            "# Step 2: Your turn -- write the add_item function\n"
            "# It should take a name and description,\n"
            "# create a dict with id, name, description, done=False,\n"
            "# and append it to the items list.\n"
            "#\n"
            "# def add_item(name, description):\n"
            "#     ... your code here ...\n"
        ).arg(m_projectDescription.left(40)));
        break;

    case Building:
        response = QString(
            "Looking good! Let me check your code and give feedback on what you've written."
        );
        break;
    }

    return response;
}
