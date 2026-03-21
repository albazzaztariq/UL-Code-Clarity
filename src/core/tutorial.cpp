#include "core/tutorial.h"

#include <QFrame>
#include <QSizePolicy>
#include <QScrollBar>

// ── Shared stylesheet constants ───────────────────────────────────────────────
static const char* DLG_BG      = "background: #1e1e2e;";
static const char* TITLE_STYLE =
    "QLabel { color: #89b4fa; font-size: 17px; font-weight: bold; background: transparent; }";
static const char* BODY_STYLE  =
    "QLabel { color: #cdd6f4; font-size: 13px; background: transparent; line-height: 1.6; }";
static const char* NAV_STYLE   =
    "QPushButton { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
    " border-radius: 6px; padding: 0 18px; font-size: 13px; min-height: 30px; }"
    "QPushButton:hover { background: #45475a; }"
    "QPushButton:disabled { background: #181825; color: #45475a; border-color: #313244; }";
static const char* CLOSE_STYLE =
    "QPushButton { background: #89b4fa; color: #1e1e2e; border: none;"
    " border-radius: 6px; padding: 0 22px; font-size: 13px; font-weight: bold; min-height: 30px; }"
    "QPushButton:hover { background: #b4d0fb; }";
static const char* DOT_ON  = "QLabel { color: #89b4fa; font-size: 18px; }";
static const char* DOT_OFF = "QLabel { color: #45475a; font-size: 18px; }";

// ── Constructor ───────────────────────────────────────────────────────────────
TutorialDialog::TutorialDialog(const QString& tutorialTitle,
                               const QVector<Page>& pages,
                               QWidget* parent)
    : QDialog(parent), m_pages(pages)
{
    setWindowTitle(tutorialTitle);
    setModal(true);
    setMinimumWidth(640);
    setMinimumHeight(480);
    setStyleSheet(DLG_BG);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 20);
    root->setSpacing(16);

    // ── Page title ────────────────────────────────────────────────────────────
    m_titleLabel = new QLabel;
    m_titleLabel->setStyleSheet(TITLE_STYLE);
    m_titleLabel->setWordWrap(true);
    root->addWidget(m_titleLabel);

    // ── Separator ─────────────────────────────────────────────────────────────
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #313244;");
    root->addWidget(sep);

    // ── Body in a scroll area ─────────────────────────────────────────────────
    m_bodyLabel = new QLabel;
    m_bodyLabel->setStyleSheet(BODY_STYLE);
    m_bodyLabel->setWordWrap(true);
    m_bodyLabel->setTextFormat(Qt::RichText);
    m_bodyLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_bodyLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidget(m_bodyLabel);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { background: #181825; width: 8px; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background: #45475a; border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    root->addWidget(m_scrollArea, 1);

    // ── Navigation dots ───────────────────────────────────────────────────────
    auto* dotsWrapper = new QWidget;
    dotsWrapper->setStyleSheet("background: transparent;");
    m_dotsLayout = new QHBoxLayout(dotsWrapper);
    m_dotsLayout->setContentsMargins(0, 0, 0, 0);
    m_dotsLayout->setSpacing(6);
    m_dotsLayout->addStretch();

    for (int i = 0; i < pages.size(); ++i) {
        auto* dot = new QLabel(QString(QChar(0x2022)));  // bullet •
        dot->setStyleSheet(DOT_OFF);
        m_dotsLayout->addWidget(dot);
        m_dots.append(dot);
    }
    m_dotsLayout->addStretch();
    root->addWidget(dotsWrapper);

    // ── Bottom button row ─────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    m_backBtn = new QPushButton("Back");
    m_backBtn->setStyleSheet(NAV_STYLE);
    m_backBtn->setCursor(Qt::PointingHandCursor);
    connect(m_backBtn, &QPushButton::clicked, this, &TutorialDialog::goBack);
    btnRow->addWidget(m_backBtn);

    btnRow->addStretch();

    auto* closeBtn = new QPushButton("Close");
    closeBtn->setStyleSheet(CLOSE_STYLE);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(closeBtn);

    m_nextBtn = new QPushButton("Next");
    m_nextBtn->setStyleSheet(NAV_STYLE);
    m_nextBtn->setCursor(Qt::PointingHandCursor);
    connect(m_nextBtn, &QPushButton::clicked, this, &TutorialDialog::goNext);
    btnRow->addWidget(m_nextBtn);

    root->addLayout(btnRow);

    showPage(0);
}

// ── Navigation ────────────────────────────────────────────────────────────────
void TutorialDialog::goNext()
{
    if (m_current < m_pages.size() - 1) {
        showPage(m_current + 1);
    }
}

void TutorialDialog::goBack()
{
    if (m_current > 0) {
        showPage(m_current - 1);
    }
}

void TutorialDialog::showPage(int index)
{
    if (index < 0 || index >= m_pages.size()) return;
    m_current = index;

    const Page& p = m_pages[index];
    m_titleLabel->setText(p.title);
    m_bodyLabel->setText(p.html);
    m_scrollArea->verticalScrollBar()->setValue(0);

    m_backBtn->setEnabled(index > 0);
    m_nextBtn->setEnabled(index < m_pages.size() - 1);

    updateDots();
}

void TutorialDialog::updateDots()
{
    for (int i = 0; i < m_dots.size(); ++i) {
        m_dots[i]->setStyleSheet(i == m_current ? DOT_ON : DOT_OFF);
    }
}

// ============================================================================
// Factory: Build Target Tutorial
// ============================================================================
TutorialDialog* TutorialDialog::buildTarget(QWidget* parent)
{
    QVector<Page> pages;

    pages.append(Page{
        "What Does \"Building\" Mean?",
        "<p>When you write code, you write instructions for a computer. But computers "
        "don't understand English or code directly — they understand <b>machine code</b>: "
        "binary instructions specific to their CPU.</p>"
        "<p><b>Building</b> (or compiling) is the process of translating your source code "
        "into something the computer can actually execute. The output might be a standalone "
        "binary (.exe on Windows, no extension on Linux/macOS), or source code in another language.</p>"
        "<p>Different languages build differently — some produce binaries ahead of time, "
        "some translate code on the fly, and some run through an interpreter.</p>"
    });

    pages.append(Page{
        "AOT Compilation — Ahead of Time",
        "<p><b>AOT (Ahead-of-Time) compilation</b> means your code is fully translated to "
        "machine code <i>before</i> it runs. The result is a <b>standalone binary</b> — "
        "a file you can double-click and it just runs, with no other software needed.</p>"
        "<p>Languages that use AOT: <b>C, C++, Rust</b>, and UniLogic when targeting native platforms.</p>"
        "<p><b>Advantages:</b> Fast execution — the CPU runs instructions directly. No startup overhead.</p>"
        "<p><b>Disadvantage:</b> The binary is tied to a specific platform "
        "(you can't take a Windows binary and run it on macOS).</p>"
    });

    pages.append(Page{
        "JIT Compilation — Just in Time",
        "<p><b>JIT (Just-in-Time) compilation</b> means the code is compiled <i>while it runs</i>. "
        "The runtime starts interpreting or compiling code on the fly as each function is called.</p>"
        "<p>Languages that use JIT: <b>Java</b> (JVM), <b>JavaScript</b> (V8 engine), "
        "<b>C#</b> (.NET CLR).</p>"
        "<p><b>Advantages:</b> Can optimise based on actual runtime behaviour. Code is more portable — "
        "the same bytecode runs anywhere a JIT runtime is installed.</p>"
        "<p><b>Disadvantages:</b> Slower startup (the JIT needs to warm up). "
        "Peak performance is usually lower than AOT-compiled code.</p>"
    });

    pages.append(Page{
        "OS and ISA — Why Downloads Ask What System You Have",
        "<p>There are two key dimensions to a platform:</p>"
        "<p><b>OS (Operating System)</b> — Windows, macOS, Linux. Each has its own "
        "system calls (ways to ask the OS to open files, create windows, connect to the network). "
        "A Windows binary calls Windows APIs. A Linux binary calls Linux APIs. "
        "They are not interchangeable.</p>"
        "<p><b>ISA (Instruction Set Architecture)</b> — the CPU's language. "
        "x64 (also called x86-64 or AMD64) is used by most Intel and AMD desktop CPUs. "
        "ARM64 (also called AArch64) is used by Apple Silicon Macs, most phones, and newer "
        "Windows laptops. A binary compiled for x64 will not run on ARM64.</p>"
        "<p><b>Platform = OS + ISA.</b> \"Windows x64\" is one platform. "
        "\"macOS ARM64\" is a different platform. This is why download pages ask "
        "which OS you have, and offer \"x64\" or \"ARM\" variants.</p>"
    });

    pages.append(Page{
        "ABI — The Contract Between Program and OS",
        "<p><b>ABI (Application Binary Interface)</b> is the low-level contract that defines "
        "how a program talks to the operating system and to other libraries at the binary level.</p>"
        "<p>It specifies things like: how function arguments are passed (in registers? on the stack?), "
        "how data structures are laid out in memory, how system calls are made, "
        "and how the stack is managed.</p>"
        "<p>This is why you can't copy a Windows <code>.exe</code> to Linux and run it — "
        "even if both machines use x64 CPUs, the ABI is completely different. "
        "The system calls, the calling conventions, the runtime libraries — all different.</p>"
        "<p>ABI compatibility is also why library updates sometimes require recompiling your program: "
        "if a library changes its ABI (how it exports functions), old compiled code breaks.</p>"
    });

    pages.append(Page{
        "Python — Write Once, Run Anywhere (With Python Installed)",
        "<p>Python is an <b>interpreted language</b>. When you run a <code>.py</code> file, "
        "the Python interpreter reads your code and executes it directly.</p>"
        "<p>Because the interpreter handles all platform differences, your <code>.py</code> file "
        "runs on Windows, macOS, or Linux without any changes — as long as Python is installed.</p>"
        "<p><b>Tradeoff:</b> Interpreted code is slower than native binaries. "
        "Every time you run the script, the interpreter re-reads and re-executes it "
        "(though Python does cache <code>.pyc</code> bytecode files to skip re-parsing).</p>"
        "<p>This portability is why Python is so popular for scripting and data science — "
        "you share a <code>.py</code> file and it just works on any machine.</p>"
    });

    pages.append(Page{
        "Java's JVM — Write Once, Run Anywhere",
        "<p>Java introduced the motto \"write once, run anywhere.\" Java source code is compiled "
        "to <b>bytecode</b> (a platform-neutral intermediate format), which runs on the "
        "<b>JVM (Java Virtual Machine)</b>.</p>"
        "<p>The JVM is available on every major OS. The same <code>.jar</code> file "
        "runs on Windows, macOS, and Linux without modification — the JVM handles "
        "platform differences.</p>"
        "<p>Several other languages also compile to JVM bytecode: "
        "<b>Kotlin</b> (Android's main language), <b>Scala</b>, <b>Groovy</b>, <b>Clojure</b>.</p>"
        "<p>The JVM uses JIT compilation — bytecode is compiled to native machine code "
        "at runtime for fast execution.</p>"
    });

    pages.append(Page{
        "WASM — WebAssembly",
        "<p><b>WebAssembly (WASM)</b> is a binary instruction format that runs in any modern web browser. "
        "You compile your code to WASM, host it on a web server, and users open your app "
        "in a browser — no install required.</p>"
        "<p>WASM runs at near-native speed and is supported by Chrome, Firefox, Safari, and Edge. "
        "Languages that can compile to WASM include C, C++, Rust, and UniLogic.</p>"
        "<p><b>Why it matters:</b> Traditionally, complex apps (games, video editors, CAD tools) "
        "required a native install. WASM lets them run in a browser tab. "
        "Figma, Google Earth, and AutoCAD web all use WASM.</p>"
        "<p>WASM is sandboxed — it can't access the filesystem or OS directly without "
        "explicit browser APIs. This makes it very secure.</p>"
    });

    pages.append(Page{
        "UniLogic VM Target — Bytecode",
        "<p>UniLogic can compile your code to <b>UL bytecode</b> — an intermediate format "
        "that runs on the UniLogic VM, similar to Python bytecode on CPython or Java bytecode on the JVM.</p>"
        "<p>The UL VM is lightweight and portable. Anywhere the VM is installed, your bytecode runs — "
        "across Windows, macOS, and Linux without recompiling.</p>"
        "<p>This is the \"distribution without commit to a single platform\" option: "
        "ship one bytecode file and let users run it on their VM.</p>"
    });

    pages.append(Page{
        "How Code Clarity Builds",
        "<p><b>OS only</b> → produces an installer that includes both x64 and ARM64 variants. "
        "The installer detects the user's ISA at install time and installs the right binary. "
        "Use this for broad distribution.</p>"
        "<p><b>OS + ISA</b> → produces a single native binary for exactly that platform. "
        "No installer — just the binary. Use this when you know your target exactly.</p>"
        "<p><b>Cannot choose ISA alone</b> — ISA is meaningless without an OS. "
        "You must choose an OS first.</p>"
        "<p><b>Bundling runtimes:</b> Code Clarity can bundle C, Python, or Rust runtimes into "
        "the output so users don't need to install them. Set runtime paths in "
        "<i>File &gt; Settings</i>. Large runtimes (like Python or Rust toolchain) are not bundled "
        "with the IDE itself — only into your output when you explicitly choose to bundle them.</p>"
    });

    return new TutorialDialog("Build Target — Full Tutorial", pages, parent);
}

// ============================================================================
// Factory: Memory Tutorial
// ============================================================================
TutorialDialog* TutorialDialog::memory(QWidget* parent)
{
    QVector<Page> pages;

    pages.append(Page{
        "RAM vs Disk",
        "<p>Your computer has two main kinds of storage:</p>"
        "<p><b>RAM (Random Access Memory)</b> — fast, temporary. "
        "Data in RAM is lost when power is cut. RAM is where your programs and data "
        "live <i>while they run</i>. Modern computers have 8–64 GB of RAM.</p>"
        "<p><b>Disk (SSD or HDD)</b> — slow, permanent. "
        "Files stay on disk when you power off. Disk is where programs are installed "
        "and where your files live permanently. Modern computers have 256 GB to several TB of disk.</p>"
        "<p>The key difference: RAM is 10–100x faster than disk for sequential reads, "
        "and millions of times faster for random access. Everything that needs to happen "
        "quickly must be in RAM.</p>"
    });

    pages.append(Page{
        "What \"Memory\" Means in Programming",
        "<p>When programmers say <b>\"memory\"</b>, they almost always mean <b>RAM</b>, "
        "not disk space. \"Memory leak\" means RAM that was allocated but never freed. "
        "\"Out of memory\" means the program ran out of RAM.</p>"
        "<p>Disk space is called \"disk\", \"storage\", or \"filesystem\" — never just \"memory\".</p>"
        "<p>This matters because the constraints are very different: "
        "you might have 16 GB of RAM but 1 TB of disk. Filling disk is easy to notice "
        "(downloads fail); filling RAM causes the system to slow to a crawl.</p>"
    });

    pages.append(Page{
        "Binary Size on Disk",
        "<p>When your program is compiled, it produces a binary file (or script) stored on disk. "
        "This is the <b>binary size</b> — how much space the program takes up as a file.</p>"
        "<p>A larger binary takes longer to download, longer to copy, and longer to initially load. "
        "For desktop apps this rarely matters. For mobile apps, web apps, or embedded systems, "
        "it matters a lot — users abandon downloads that take too long.</p>"
        "<p>Binary size is influenced by: how much code you have, whether debug symbols are included, "
        "which libraries are statically linked in, and whether the binary is stripped "
        "(debug info removed for release builds).</p>"
        "<p>Code Clarity shows you the binary size before you run the program, "
        "so you can spot unexpectedly large outputs early.</p>"
    });

    pages.append(Page{
        "RAM at Runtime",
        "<p>When your program runs, the OS loads it into RAM and starts executing. "
        "Variables, objects, arrays, strings — everything your program creates at runtime "
        "lives in RAM.</p>"
        "<p>RAM usage grows as your program creates more data and shrinks as it frees data. "
        "How it grows and shrinks depends on the language:</p>"
        "<p>- <b>C / C++:</b> Manual control. You call <code>malloc()</code> to allocate "
        "and <code>free()</code> to release. Forget to free = memory leak.</p>"
        "<p>- <b>Python / Java / Go:</b> Garbage collected. The runtime automatically finds "
        "and frees data that's no longer reachable.</p>"
        "<p>- <b>Rust:</b> Ownership model. The compiler tracks lifetimes and inserts "
        "free calls automatically — no GC, no leaks.</p>"
    });

    pages.append(Page{
        "Demand Paging — Your Program Is NOT Fully in RAM",
        "<p>Here is something that surprises many developers: <b>your entire program is not "
        "loaded into RAM at once.</b></p>"
        "<p>The OS uses a technique called <b>demand paging</b> (also called virtual memory). "
        "RAM is divided into 4 KB chunks called <b>pages</b>. The OS only loads a page into RAM "
        "when your program actually tries to access it. Everything else stays on disk.</p>"
        "<p>If you have a 100 MB program but your current execution path only touches "
        "2 MB of code and data, only those pages are in RAM. The rest are on disk, "
        "waiting to be loaded if ever needed.</p>"
        "<p>This is why programs can run even if they're larger than available RAM — "
        "the OS swaps pages in and out as needed. But if too many pages are needed at once, "
        "the OS frantically swaps pages between RAM and disk. "
        "This is called <b>thrashing</b> and it makes everything slow.</p>"
    });

    pages.append(Page{
        "Why Memory Efficiency Matters",
        "<p>If your program uses more RAM than the system has available, the OS starts "
        "<b>swapping</b> — moving pages from RAM to disk to make room. "
        "Disk access is millions of times slower than RAM, so performance collapses.</p>"
        "<p>On mobile and embedded systems, there's often no swap at all. "
        "If you exceed available RAM, the OS kills your process outright (OOM killer).</p>"
        "<p>Memory efficiency also matters for server software: "
        "if each user session uses 50 MB of RAM, a 16 GB server handles only ~320 concurrent users. "
        "Cut it to 5 MB and you serve 3,200 users on the same hardware.</p>"
        "<p>Code Clarity tracks your program's RAM usage over time so you can catch "
        "leaks, unexpected spikes, and runaway growth before they hit production.</p>"
    });

    pages.append(Page{
        "Stack vs Heap",
        "<p>When your program runs, RAM is divided into regions. The two most important are:</p>"
        "<p><b>Stack</b> — fast, automatic. Local variables in a function live on the stack. "
        "When a function returns, its stack frame is popped and that memory is instantly reclaimed. "
        "No bookkeeping needed. The stack is small (typically 1–8 MB per thread).</p>"
        "<p><b>Heap</b> — flexible, manual (or GC'd). Objects you create dynamically "
        "(<code>new</code> in C++/Java, <code>malloc</code> in C, all Python objects) live on the heap. "
        "The heap can grow to fill available RAM, but allocation is slower than the stack "
        "and the memory must be explicitly freed (or garbage collected).</p>"
        "<p><b>Python:</b> almost everything goes on the heap — even small integers. "
        "This is convenient but costs RAM. "
        "<b>C:</b> local variables go on the stack, heap only when you call <code>malloc()</code>. "
        "This is efficient but requires discipline.</p>"
    });

    return new TutorialDialog("Memory — Full Tutorial", pages, parent);
}

// ============================================================================
// Factory: Security Tutorial
// ============================================================================
TutorialDialog* TutorialDialog::security(QWidget* parent)
{
    QVector<Page> pages;

    pages.append(Page{
        "What Is a Vulnerability?",
        "<p>A <b>vulnerability</b> is a weakness in code that an attacker can exploit. "
        "It might be a bug, a missing check, an unsafe assumption, or a dangerous pattern.</p>"
        "<p>Vulnerabilities are the standard term in the security industry. "
        "When you hear about a CVE (Common Vulnerabilities and Exposures number), "
        "it's a publicly disclosed vulnerability in a piece of software.</p>"
        "<p>Most vulnerabilities don't come from malicious developers — they come from "
        "honest mistakes: forgetting to check the length of input, trusting data from the network, "
        "or using an unsafe function that's been known to be dangerous for decades.</p>"
        "<p>The goal of security testing is to find these weaknesses before attackers do.</p>"
    });

    pages.append(Page{
        "How Attackers Exploit Applications",
        "<p>A typical attack follows this pattern:</p>"
        "<p><b>1. Find a vulnerability</b> — automated scanners, manual code review, "
        "or checking known CVE databases for the libraries you use.</p>"
        "<p><b>2. Craft a malicious input</b> — a specially constructed string, file, "
        "or network packet designed to trigger the weakness.</p>"
        "<p><b>3. Achieve the goal:</b></p>"
        "<p style='margin-left:20px;'>- <b>Read data</b> — steal passwords, API keys, user data</p>"
        "<p style='margin-left:20px;'>- <b>Modify data</b> — change records, corrupt files</p>"
        "<p style='margin-left:20px;'>- <b>Execute code</b> — run arbitrary commands on the server</p>"
        "<p style='margin-left:20px;'>- <b>Crash the system</b> — denial of service</p>"
        "<p>Good security testing simulates steps 1 and 2 in a controlled way "
        "so you can fix weaknesses before real attackers find them.</p>"
    });

    pages.append(Page{
        "Buffer Overflows",
        "<p>A <b>buffer overflow</b> occurs when code writes more data into a fixed-size buffer "
        "than the buffer can hold, overwriting adjacent memory.</p>"
        "<p>In C and C++, buffers are just contiguous memory regions. "
        "If you have a 64-byte array and write 128 bytes into it, "
        "the extra 64 bytes overwrite whatever is next in memory — "
        "which might be another variable, a saved register, or the return address on the stack.</p>"
        "<p>An attacker can craft input that overwrites the return address with the address of "
        "attacker-controlled code. When the function returns, execution jumps to the attacker's code. "
        "This is called a <b>stack smashing attack</b> and is the classic buffer overflow exploit.</p>"
        "<p>Buffer overflows are the <b>#1 source of security vulnerabilities in C/C++ code</b>. "
        "Modern mitigations (stack canaries, ASLR, NX bits) make exploitation harder but not impossible.</p>"
        "<p>Rust's ownership model and bounds checking prevent buffer overflows entirely at compile time.</p>"
    });

    pages.append(Page{
        "Out-of-Memory (OOM) Errors",
        "<p>An <b>OOM (Out-of-Memory) error</b> happens when a program tries to allocate more RAM "
        "than the OS can provide.</p>"
        "<p>On Linux, the OOM killer selects a process and kills it to reclaim memory. "
        "On Windows and macOS, allocation fails or the system begins swapping heavily.</p>"
        "<p>From a security perspective, OOM errors can be <b>triggered intentionally</b> by an attacker. "
        "If your program reads a file and allocates a buffer equal to the file's claimed size, "
        "an attacker can send a file claiming to be 10 TB in size. "
        "Your program tries to allocate 10 TB, the OS kills it, and your service is down. "
        "This is a <b>denial of service (DoS)</b> attack.</p>"
        "<p>The fix: always validate claimed sizes before allocating. "
        "Never trust user-supplied lengths without limits.</p>"
    });

    pages.append(Page{
        "SQL Injection",
        "<p><b>SQL injection</b> is one of the most common and dangerous web vulnerabilities. "
        "It occurs when user-supplied input is inserted directly into a database query "
        "without being sanitised.</p>"
        "<p>Example — a login query built by string concatenation:</p>"
        "<p><code style='color: #f9e2af;'>SELECT * FROM users WHERE name='\" + username + \"'</code></p>"
        "<p>If the user types <code>' OR '1'='1</code> as their username, the query becomes:</p>"
        "<p><code style='color: #f38ba8;'>SELECT * FROM users WHERE name='' OR '1'='1'</code></p>"
        "<p>This returns all users — the attacker is now logged in as the first user in the database, "
        "typically an admin.</p>"
        "<p><b>Fix:</b> Always use parameterised queries (prepared statements). "
        "Never build SQL by concatenating strings with user input.</p>"
    });

    pages.append(Page{
        "Code Injection",
        "<p><b>Code injection</b> happens when user input is executed as code. "
        "The classic examples are functions like <code>eval()</code>, <code>exec()</code>, "
        "and <code>os.system()</code> in Python, or <code>eval()</code> in JavaScript.</p>"
        "<p>If your program does:</p>"
        "<p><code style='color: #f9e2af;'>os.system(\"ping \" + user_input)</code></p>"
        "<p>An attacker enters <code>google.com; rm -rf /</code> and your server deletes itself.</p>"
        "<p>The same applies to shell commands built from user input, "
        "template engines that execute code, and any deserialization of untrusted data.</p>"
        "<p><b>Fix:</b> Never pass user input to functions that execute code or shell commands. "
        "Use library functions with argument arrays instead of shell strings "
        "(<code>subprocess.run([\"ping\", user_input])</code> is safe; "
        "<code>os.system(\"ping \" + user_input)</code> is not).</p>"
    });

    pages.append(Page{
        "SAST — Static Application Security Testing",
        "<p><b>SAST</b> analyzes your source code <i>without running it</i>. "
        "It reads your code line by line, looking for known dangerous patterns, "
        "missing checks, and calls to unsafe functions.</p>"
        "<p>Think of it as a security expert reviewing your code. "
        "It can find buffer overflows, SQL injection, code injection, hardcoded secrets, "
        "missing input validation, and more — all without needing to run the program.</p>"
        "<p><b>Advantages:</b> Runs fast, works on incomplete code, catches issues early in development.</p>"
        "<p><b>Limitations:</b> Can produce false positives (flagging safe code as dangerous) "
        "and can miss runtime-only issues (bugs that only appear with specific inputs).</p>"
        "<p>In Code Clarity, SAST runs on the <b>Static Analysis</b> tab.</p>"
    });

    pages.append(Page{
        "DAST — Dynamic Application Security Testing",
        "<p><b>DAST</b> runs your compiled program and sends it malicious inputs to see what breaks. "
        "Think of it as a controlled hacking attempt — your own program being attacked in a safe environment.</p>"
        "<p>DAST sends attack inputs like buffer-overflow strings, SQL injection payloads, "
        "extremely large inputs, and malformed data. It watches for crashes, unexpected output, "
        "memory errors, and information leaks.</p>"
        "<p><b>Advantages:</b> Finds real vulnerabilities that only appear at runtime. "
        "No false positives — if the program crashes on a specific input, that's a real bug.</p>"
        "<p><b>Limitations:</b> Requires a compiled executable. Can't find issues in code paths "
        "not exercised by the test inputs. Slower than SAST.</p>"
        "<p>In Code Clarity, DAST runs on the <b>Dynamic Testing</b> tab. "
        "Build your program first, then run DAST.</p>"
    });

    pages.append(Page{
        "SAST and DAST Together",
        "<p>SAST and DAST are complementary — neither is sufficient alone.</p>"
        "<p><b>SAST</b> catches issues early, works without running the program, "
        "and scans 100% of code paths. Use it continuously during development.</p>"
        "<p><b>DAST</b> confirms real exploitability and finds runtime-only issues "
        "that static analysis misses. Use it before releases and on CI/CD pipelines.</p>"
        "<p>Professional security teams use both, plus manual code review and penetration testing. "
        "For most projects, SAST + DAST catches the majority of common vulnerabilities.</p>"
        "<p>The security testing tools in this window implement both: "
        "the <b>Static Analysis</b> tab on the left runs SAST, "
        "the <b>Dynamic Testing</b> tab on the right runs DAST.</p>"
    });

    return new TutorialDialog("Security — Full Tutorial", pages, parent);
}
