#include "core/securitylab.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QMessageBox>
#include <QSizePolicy>
#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStackedWidget>
#include <QGroupBox>
#include <QTimer>
#include <QTextCursor>

// ── Shared style helpers ──────────────────────────────────────────────────────

static QString btnStyle(const QString& bg, const QString& hoverBg,
                        const QString& fg = "#1e1e2e")
{
    return QString(
        "QPushButton { background: %1; color: %3; border: none;"
        " border-radius: 4px; padding: 0 14px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: %2; }"
        "QPushButton:disabled { background: #313244; color: #6c7086; }")
        .arg(bg, hoverBg, fg);
}

// ── Lab program paths ─────────────────────────────────────────────────────────
// Labs live next to the executable in a "labs/" subfolder at runtime.

static QString labsDir()
{
    // Prefer next to executable (deployed build)
    QString exeDir = QApplication::applicationDirPath();
    QString candidate = exeDir + "/labs";
    if (QDir(candidate).exists()) return candidate;

    // Fall back to source tree (development)
    // Walk up from exe until we find src/labs
    QDir d(exeDir);
    for (int i = 0; i < 6; ++i) {
        QString src = d.absolutePath() + "/src/labs";
        if (QDir(src).exists()) return src;
        if (!d.cdUp()) break;
    }
    return candidate; // best guess even if not found
}

// ── Built-in LabDefinitions ───────────────────────────────────────────────────

static LabDefinition makeBufferOverflow()
{
    QString dir = labsDir();
    LabDefinition d;
    d.name        = "Buffer Overflow";
    d.vulnType    = "buffer_overflow";
    d.isPython    = false;
    d.labProgram  = dir + "/buffer_overflow";     // compiled .exe on Windows
    d.labProgram2 = dir + "/buffer_overflow_fix";
    d.normalInput = "Hello!";
    d.attackInput = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    d.vulnCode    =
        "#include <stdio.h>\n"
        "#include <string.h>\n"
        "\n"
        "int main(void) {\n"
        "    char buf[8];          // only 8 bytes on the stack\n"
        "    printf(\"Enter input: \");\n"
        "    gets(buf);            // UNSAFE: reads unlimited bytes!\n"
        "    printf(\"You said: %s\\n\", buf);\n"
        "    return 0;\n"
        "}\n";
    d.fixedCode   =
        "#include <stdio.h>\n"
        "#include <string.h>\n"
        "\n"
        "int main(void) {\n"
        "    char buf[8];\n"
        "    printf(\"Enter input: \");\n"
        "    fgets(buf, sizeof(buf), stdin);  // SAFE: length-bounded\n"
        "    // strip trailing newline\n"
        "    size_t len = strlen(buf);\n"
        "    if (len > 0 && buf[len-1] == '\\n') buf[len-1] = '\\0';\n"
        "    printf(\"You said: %s\\n\", buf);\n"
        "    return 0;\n"
        "}\n";
    d.explanation =
        "gets() reads input with no length limit. You wrote more bytes than "
        "the buffer can hold. The extra bytes overwrite the return address on "
        "the stack — the value the CPU jumps to when the function returns. "
        "In a real attack those bytes would point to the attacker's shellcode, "
        "giving them full control of the process.";
    d.fixExplanation =
        "fgets() takes a length argument. It reads at most sizeof(buf)-1 characters, "
        "leaving room for the null terminator. No matter what the attacker types, "
        "the buffer cannot overflow.";
    return d;
}

static LabDefinition makeUseAfterFree()
{
    QString dir = labsDir();
    LabDefinition d;
    d.name        = "Use After Free";
    d.vulnType    = "use_after_free";
    d.isPython    = false;
    d.labProgram  = dir + "/uaf_demo";
    d.labProgram2 = dir + "/uaf_demo_fix";
    d.normalInput = "";
    d.attackInput = "";
    d.vulnCode    =
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <string.h>\n"
        "\n"
        "int main(void) {\n"
        "    char* secret = malloc(32);\n"
        "    strcpy(secret, \"password=hunter2\");\n"
        "\n"
        "    free(secret);        // memory returned to allocator\n"
        "\n"
        "    char* reuse = malloc(32);       // same block reused!\n"
        "    strcpy(reuse, \"ATTACKER_DATA\");\n"
        "\n"
        "    printf(\"%s\\n\", secret);  // BUG: secret is freed!\n"
        "    free(reuse);\n"
        "}\n";
    d.fixedCode   =
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <string.h>\n"
        "\n"
        "int main(void) {\n"
        "    char* secret = malloc(32);\n"
        "    strcpy(secret, \"password=hunter2\");\n"
        "\n"
        "    free(secret);\n"
        "    secret = NULL;       // FIX: nullify immediately\n"
        "\n"
        "    if (secret != NULL) {\n"
        "        printf(\"%s\\n\", secret);\n"
        "    } else {\n"
        "        printf(\"[guard] pointer is NULL — use prevented\\n\");\n"
        "    }\n"
        "}\n";
    d.explanation =
        "After free(), the pointer variable still holds the old address. "
        "The allocator may give that memory to a different part of the program "
        "— or to a different request entirely. Reading through the freed pointer "
        "returns whatever was written there since, which could be attacker-controlled "
        "data. Writing through it silently corrupts live data structures.";
    d.fixExplanation =
        "Set the pointer to NULL immediately after free(). Any subsequent "
        "dereference crashes instantly (null dereference) — a loud, easy-to-find "
        "bug — rather than silently corrupting data in unpredictable ways.";
    return d;
}

static LabDefinition makeSqlInjection()
{
    QString dir = labsDir();
    LabDefinition d;
    d.name        = "SQL Injection";
    d.vulnType    = "sql_injection";
    d.isPython    = true;
    d.labProgram  = dir + "/sqli_demo.py";
    d.labProgram2 = dir + "/sqli_demo_fix.py";
    d.normalInput = "alice\npassword123";
    d.attackInput = "' OR '1'='1\nanypassword";
    d.vulnCode    =
        "import sqlite3\n"
        "\n"
        "def login(username, password):\n"
        "    # VULNERABLE: string concatenation\n"
        "    query = (\"SELECT * FROM users \"\n"
        "             \"WHERE username='\" + username + \"'\"\n"
        "             \" AND password='\" + password + \"'\")\n"
        "    cursor.execute(query)   # attacker controls query!\n"
        "    return cursor.fetchall()\n";
    d.fixedCode   =
        "import sqlite3\n"
        "\n"
        "def login(username, password):\n"
        "    # FIXED: parameterized query — ? are placeholders\n"
        "    query = (\"SELECT * FROM users \"\n"
        "             \"WHERE username=? AND password=?\")\n"
        "    cursor.execute(query, (username, password))  # safe!\n"
        "    return cursor.fetchall()\n";
    d.explanation =
        "The username field was pasted directly into the SQL string. "
        "Typing  ' OR '1'='1  closes the string early and adds a condition "
        "that is always true — so the database returns every user row. "
        "The attacker logs in as the first user (usually admin) without "
        "knowing any password. SQL injection is the #1 web vulnerability.";
    d.fixExplanation =
        "Parameterized queries (also called prepared statements) separate code "
        "from data. The ? placeholders are filled in by the database engine after "
        "the query is already compiled — so quotes in the input are treated as "
        "literal characters, never as SQL syntax.";
    return d;
}

static LabDefinition makeXss()
{
    QString dir = labsDir();
    LabDefinition d;
    d.name        = "Cross-Site Scripting (XSS)";
    d.vulnType    = "xss";
    d.isPython    = true;
    d.labProgram  = dir + "/xss_demo.py";
    d.labProgram2 = dir + "/xss_demo_fix.py";
    d.normalInput = "Hello, great article!";
    d.attackInput = "<script>document.cookie</script>";
    d.vulnCode    =
        "// JavaScript — vulnerable page\n"
        "function postComment(text) {\n"
        "    // VULNERABLE: treats input as HTML\n"
        "    document.getElementById('comment')\n"
        "            .innerHTML = text;\n"
        "    //  ^ browser EXECUTES <script> tags!\n"
        "}\n";
    d.fixedCode   =
        "// JavaScript — fixed page\n"
        "function postComment(text) {\n"
        "    // FIXED: textContent — always plain text\n"
        "    document.getElementById('comment')\n"
        "            .textContent = text;\n"
        "    //  ^ browser DISPLAYS <script> as text\n"
        "}\n"
        "\n"
        "// Server-side (Python):\n"
        "import html\n"
        "safe = html.escape(user_input)   # &lt;script&gt;\n";
    d.explanation =
        "innerHTML tells the browser to parse the string as HTML. "
        "A <script> tag is executed in the visitor's browser with full "
        "access to their cookies, local storage, and DOM. The attacker can "
        "steal session tokens (logging in as the victim), redirect to phishing "
        "pages, or mine cryptocurrency — all silently, in every visitor's browser.";
    d.fixExplanation =
        "textContent treats the value as plain text — angle brackets are displayed "
        "literally, never parsed as HTML. Server-side: html.escape() converts < to "
        "&lt; so even if innerHTML is used, the browser shows it as text.";
    return d;
}

static LabDefinition makeOom()
{
    QString dir = labsDir();
    LabDefinition d;
    d.name        = "Out of Memory (OOM)";
    d.vulnType    = "oom";
    d.isPython    = true;
    d.labProgram  = dir + "/oom_demo.py";
    d.labProgram2 = dir + "/oom_demo.py";
    d.normalInput = "normal";
    d.attackInput = "attack";
    d.vulnCode    =
        "# VULNERABLE: unbounded allocation\n"
        "data = []\n"
        "while True:\n"
        "    # each iteration: +1 MB\n"
        "    data.append('A' * 1_000_000)\n"
        "    # no exit condition — runs forever!\n"
        "    # OS kills process when RAM exhausted\n";
    d.fixedCode   =
        "# FIXED: bounded allocation with cleanup\n"
        "MAX_MB = 100\n"
        "data   = []\n"
        "\n"
        "for _ in range(MAX_MB):\n"
        "    data.append('A' * 1_000_000)\n"
        "\n"
        "# Process data here...\n"
        "process(data)\n"
        "\n"
        "# Release when done\n"
        "del data\n"
        "gc.collect()\n";
    d.explanation =
        "The loop allocates 1 MB per iteration with no upper bound. "
        "A single attacker request can exhaust all RAM on the server, "
        "causing the OS to kill the process (or the whole system to thrash). "
        "Every other user loses access instantly — a classic Denial of Service "
        "(DoS) attack exploiting missing resource limits.";
    d.fixExplanation =
        "Set an explicit cap on how much memory a single request can use. "
        "Process data in fixed-size chunks and release memory when done. "
        "On server endpoints, enforce per-request memory limits at the framework "
        "or OS level (e.g. resource.setrlimit on Linux, ulimit).";
    return d;
}

// ── allLabs / forVulnType ─────────────────────────────────────────────────────

QList<LabDefinition> SecurityLabWidget::allLabs()
{
    return {
        makeBufferOverflow(),
        makeUseAfterFree(),
        makeSqlInjection(),
        makeXss(),
        makeOom(),
    };
}

SecurityLabWidget* SecurityLabWidget::forVulnType(const QString& vulnType, QWidget* parent)
{
    for (const auto& lab : allLabs()) {
        if (lab.vulnType == vulnType)
            return new SecurityLabWidget(lab, parent);
    }
    return nullptr;
}

// ── Constructor ───────────────────────────────────────────────────────────────

SecurityLabWidget::SecurityLabWidget(const LabDefinition& lab, QWidget* parent)
    : QWidget(parent), m_lab(lab)
{
    setStyleSheet("background: #1e1e2e; color: #cdd6f4;");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(8);

    // ── Top bar ──────────────────────────────────────────────────────────────
    auto* topBar = new QHBoxLayout;

    auto* closeBtn = new QPushButton("Back");
    closeBtn->setFixedHeight(28);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; padding: 0 12px; font-size: 12px; }"
        "QPushButton:hover { background: #45475a; }");
    connect(closeBtn, &QPushButton::clicked, this, &SecurityLabWidget::closeRequested);
    topBar->addWidget(closeBtn);

    topBar->addSpacing(10);

    auto* titleLabel = new QLabel(QString("Lab: %1").arg(lab.name));
    titleLabel->setStyleSheet("color: #cdd6f4; font-size: 16px; font-weight: bold;");
    topBar->addWidget(titleLabel);

    topBar->addStretch(1);

    m_statusLabel = new QLabel;
    m_statusLabel->setStyleSheet("color: #a6adc8; font-size: 12px;");
    topBar->addWidget(m_statusLabel);

    root->addLayout(topBar);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #313244;");
    root->addWidget(sep);

    // ── Action buttons ───────────────────────────────────────────────────────
    auto* btnBar = new QHBoxLayout;
    btnBar->setSpacing(8);

    m_runNormalBtn = new QPushButton("Run Normal Input");
    m_runNormalBtn->setFixedHeight(30);
    m_runNormalBtn->setCursor(Qt::PointingHandCursor);
    m_runNormalBtn->setStyleSheet(btnStyle("#a6e3a1", "#c3f5bf"));
    connect(m_runNormalBtn, &QPushButton::clicked, this, &SecurityLabWidget::runNormal);
    btnBar->addWidget(m_runNormalBtn);

    m_runAttackBtn = new QPushButton("Run Attack Input");
    m_runAttackBtn->setFixedHeight(30);
    m_runAttackBtn->setCursor(Qt::PointingHandCursor);
    m_runAttackBtn->setStyleSheet(btnStyle("#f38ba8", "#f5a9bc"));
    connect(m_runAttackBtn, &QPushButton::clicked, this, &SecurityLabWidget::runAttack);
    btnBar->addWidget(m_runAttackBtn);

    m_fixBtn = new QPushButton("Show the Fix");
    m_fixBtn->setFixedHeight(30);
    m_fixBtn->setCursor(Qt::PointingHandCursor);
    m_fixBtn->setStyleSheet(btnStyle("#89b4fa", "#b4d0fb"));
    connect(m_fixBtn, &QPushButton::clicked, this, &SecurityLabWidget::showFix);
    btnBar->addWidget(m_fixBtn);

    m_whyBtn = new QPushButton("Why This Matters");
    m_whyBtn->setFixedHeight(30);
    m_whyBtn->setCursor(Qt::PointingHandCursor);
    m_whyBtn->setStyleSheet(btnStyle("#f9e2af", "#fbefc5"));
    connect(m_whyBtn, &QPushButton::clicked, this, &SecurityLabWidget::showWhyItMatters);
    btnBar->addWidget(m_whyBtn);

    btnBar->addStretch(1);
    root->addLayout(btnBar);

    // ── 3-pane splitter ──────────────────────────────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(2);
    splitter->setStyleSheet("QSplitter::handle { background: #313244; }");

    // ── LEFT: Code pane ──────────────────────────────────────────────────────
    auto* codeContainer = new QWidget;
    codeContainer->setStyleSheet("background: #181825;");
    auto* codeLayout = new QVBoxLayout(codeContainer);
    codeLayout->setContentsMargins(0, 0, 0, 0);
    codeLayout->setSpacing(0);

    auto* codeHeader = new QLabel("  Code");
    codeHeader->setFixedHeight(24);
    codeHeader->setStyleSheet(
        "background: #313244; color: #a6adc8; font-size: 11px; font-weight: bold;"
        " border-bottom: 1px solid #45475a; padding-left: 6px;");
    codeLayout->addWidget(codeHeader);

    m_codeStack = new QStackedWidget;

    QFont monoFont("Consolas", 11);
    monoFont.setFixedPitch(true);

    m_codePaneVuln = new QPlainTextEdit;
    m_codePaneVuln->setReadOnly(true);
    m_codePaneVuln->setFont(monoFont);
    m_codePaneVuln->setPlainText(lab.vulnCode);
    m_codePaneVuln->setStyleSheet(
        "QPlainTextEdit { background: #181825; color: #cdd6f4; border: none; padding: 6px; }"
        "QScrollBar:vertical { background: #1e1e2e; width: 8px; }"
        "QScrollBar::handle:vertical { background: #45475a; border-radius: 3px; }");
    m_codeStack->addWidget(m_codePaneVuln);

    m_codePaneFix = new QPlainTextEdit;
    m_codePaneFix->setReadOnly(true);
    m_codePaneFix->setFont(monoFont);
    m_codePaneFix->setPlainText(lab.fixedCode);
    m_codePaneFix->setStyleSheet(
        "QPlainTextEdit { background: #1a2b1a; color: #a6e3a1; border: none; padding: 6px; }"
        "QScrollBar:vertical { background: #1e1e2e; width: 8px; }"
        "QScrollBar::handle:vertical { background: #45475a; border-radius: 3px; }");
    m_codeStack->addWidget(m_codePaneFix);

    m_codeStack->setCurrentIndex(0);
    codeLayout->addWidget(m_codeStack, 1);

    splitter->addWidget(codeContainer);

    // ── CENTER: Input pane ───────────────────────────────────────────────────
    auto* inputContainer = new QWidget;
    inputContainer->setStyleSheet("background: #1e1e2e;");
    auto* inputLayout = new QVBoxLayout(inputContainer);
    inputLayout->setContentsMargins(8, 8, 8, 8);
    inputLayout->setSpacing(8);

    auto* inputHeader = new QLabel("Input");
    inputHeader->setStyleSheet(
        "color: #a6adc8; font-size: 11px; font-weight: bold; padding: 0;");
    inputLayout->addWidget(inputHeader);

    // SQL and XSS get a mock webpage UI; others get a plain text field
    bool isSql = (lab.vulnType == "sql_injection");
    bool isXss = (lab.vulnType == "xss");

    if (isSql) {
        buildSqlMockUi(inputContainer, inputLayout);
    } else if (isXss) {
        buildXssMockUi(inputContainer, inputLayout);
    } else {
        // Generic input field
        auto* inputLabel = new QLabel("Program input:");
        inputLabel->setStyleSheet("color: #a6adc8; font-size: 12px;");
        inputLayout->addWidget(inputLabel);

        m_inputField = new QLineEdit;
        m_inputField->setText(lab.normalInput);
        m_inputField->setFont(monoFont);
        m_inputField->setStyleSheet(
            "QLineEdit { background: #181825; color: #cdd6f4; border: 1px solid #45475a;"
            " border-radius: 4px; padding: 6px; font-size: 13px; }");
        inputLayout->addWidget(m_inputField);

        auto* hint = new QLabel(
            QString("Normal: %1\nAttack: %2")
            .arg(lab.normalInput, lab.attackInput));
        hint->setStyleSheet("color: #6c7086; font-size: 11px;");
        hint->setWordWrap(true);
        inputLayout->addWidget(hint);
    }

    inputLayout->addStretch(1);
    splitter->addWidget(inputContainer);

    // ── RIGHT: Output pane ───────────────────────────────────────────────────
    auto* outputContainer = new QWidget;
    outputContainer->setStyleSheet("background: #181825;");
    auto* outputLayout = new QVBoxLayout(outputContainer);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setSpacing(0);

    auto* outputHeader = new QLabel("  Output / Shell");
    outputHeader->setFixedHeight(24);
    outputHeader->setStyleSheet(
        "background: #313244; color: #a6adc8; font-size: 11px; font-weight: bold;"
        " border-bottom: 1px solid #45475a; padding-left: 6px;");
    outputLayout->addWidget(outputHeader);

    m_outputPane = new QTextEdit;
    m_outputPane->setReadOnly(true);
    m_outputPane->setFont(monoFont);
    m_outputPane->setStyleSheet(
        "QTextEdit { background: #181825; color: #cdd6f4; border: none; padding: 6px; }"
        "QScrollBar:vertical { background: #1e1e2e; width: 8px; }"
        "QScrollBar::handle:vertical { background: #45475a; border-radius: 3px; }");
    outputLayout->addWidget(m_outputPane, 1);

    splitter->addWidget(outputContainer);

    splitter->setSizes({380, 260, 360});
    root->addWidget(splitter, 1);

    // ── Fix explanation label ─────────────────────────────────────────────────
    // (shown below the splitter when fix mode is active — initially hidden)
    m_statusLabel->setText("Press a Run button to see what happens.");
}

// ── SQL mock UI ───────────────────────────────────────────────────────────────

void SecurityLabWidget::buildSqlMockUi(QWidget* /*parent*/, QVBoxLayout* layout)
{
    // Styled box that looks like a login form
    auto* frame = new QFrame;
    frame->setStyleSheet(
        "QFrame { background: #2a2a3c; border: 1px solid #45475a; border-radius: 6px; }");
    auto* fl = new QVBoxLayout(frame);
    fl->setContentsMargins(14, 12, 14, 12);
    fl->setSpacing(10);

    auto* siteTitle = new QLabel("Login");
    siteTitle->setStyleSheet(
        "color: #cdd6f4; font-size: 15px; font-weight: bold; border: none;");
    fl->addWidget(siteTitle);

    auto* unLabel = new QLabel("Username");
    unLabel->setStyleSheet("color: #a6adc8; font-size: 12px; border: none;");
    fl->addWidget(unLabel);

    m_sqlUsername = new QLineEdit;
    m_sqlUsername->setText(m_lab.normalInput.split('\n').value(0));
    m_sqlUsername->setStyleSheet(
        "QLineEdit { background: #181825; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; padding: 6px; font-size: 13px; }");
    fl->addWidget(m_sqlUsername);

    auto* pwLabel = new QLabel("Password");
    pwLabel->setStyleSheet("color: #a6adc8; font-size: 12px; border: none;");
    fl->addWidget(pwLabel);

    m_sqlPassword = new QLineEdit;
    m_sqlPassword->setEchoMode(QLineEdit::Password);
    m_sqlPassword->setText(m_lab.normalInput.split('\n').value(1, "password123"));
    m_sqlPassword->setStyleSheet(
        "QLineEdit { background: #181825; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; padding: 6px; font-size: 13px; }");
    fl->addWidget(m_sqlPassword);

    auto* loginBtn = new QPushButton("Login");
    loginBtn->setFixedHeight(32);
    loginBtn->setCursor(Qt::PointingHandCursor);
    loginBtn->setStyleSheet(btnStyle("#89b4fa", "#b4d0fb"));
    connect(loginBtn, &QPushButton::clicked, this, [this]() {
        QString input = m_sqlUsername->text() + "\n" + m_sqlPassword->text();
        runWithInput(input);
    });
    fl->addWidget(loginBtn);

    layout->addWidget(frame);

    auto* hint = new QLabel(
        "Try injecting:  ' OR '1'='1\ninto the Username field, then press Login");
    hint->setStyleSheet("color: #6c7086; font-size: 11px;");
    hint->setWordWrap(true);
    layout->addWidget(hint);
}

// ── XSS mock UI ──────────────────────────────────────────────────────────────

void SecurityLabWidget::buildXssMockUi(QWidget* /*parent*/, QVBoxLayout* layout)
{
    auto* frame = new QFrame;
    frame->setStyleSheet(
        "QFrame { background: #2a2a3c; border: 1px solid #45475a; border-radius: 6px; }");
    auto* fl = new QVBoxLayout(frame);
    fl->setContentsMargins(14, 12, 14, 12);
    fl->setSpacing(10);

    auto* siteTitle = new QLabel("Post a Comment");
    siteTitle->setStyleSheet(
        "color: #cdd6f4; font-size: 15px; font-weight: bold; border: none;");
    fl->addWidget(siteTitle);

    auto* cmtLabel = new QLabel("Your comment:");
    cmtLabel->setStyleSheet("color: #a6adc8; font-size: 12px; border: none;");
    fl->addWidget(cmtLabel);

    m_xssComment = new QTextEdit;
    m_xssComment->setFixedHeight(80);
    m_xssComment->setText(m_lab.normalInput);
    m_xssComment->setStyleSheet(
        "QTextEdit { background: #181825; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; padding: 6px; font-size: 13px; }");
    fl->addWidget(m_xssComment);

    auto* postBtn = new QPushButton("Post Comment");
    postBtn->setFixedHeight(32);
    postBtn->setCursor(Qt::PointingHandCursor);
    postBtn->setStyleSheet(btnStyle("#89b4fa", "#b4d0fb"));
    connect(postBtn, &QPushButton::clicked, this, [this]() {
        runWithInput(m_xssComment->toPlainText());
    });
    fl->addWidget(postBtn);

    layout->addWidget(frame);

    auto* hint = new QLabel(
        "Try injecting:  <script>document.cookie</script>\ninto the comment box");
    hint->setStyleSheet("color: #6c7086; font-size: 11px;");
    hint->setWordWrap(true);
    layout->addWidget(hint);
}

// ── Run buttons ───────────────────────────────────────────────────────────────

void SecurityLabWidget::runNormal()
{
    // Update mock UI fields if present
    if (m_sqlUsername) {
        QStringList parts = m_lab.normalInput.split('\n');
        m_sqlUsername->setText(parts.value(0));
        m_sqlPassword->setText(parts.value(1, "password123"));
        runWithInput(m_lab.normalInput);
        return;
    }
    if (m_xssComment) {
        m_xssComment->setText(m_lab.normalInput);
        runWithInput(m_lab.normalInput);
        return;
    }
    QString input = m_inputField ? m_inputField->text() : m_lab.normalInput;
    runWithInput(input.isEmpty() ? m_lab.normalInput : input);
}

void SecurityLabWidget::runAttack()
{
    if (m_sqlUsername) {
        QStringList parts = m_lab.attackInput.split('\n');
        m_sqlUsername->setText(parts.value(0));
        m_sqlPassword->setText(parts.value(1, "anything"));
        runWithInput(m_lab.attackInput);
        return;
    }
    if (m_xssComment) {
        m_xssComment->setText(m_lab.attackInput);
        runWithInput(m_lab.attackInput);
        return;
    }
    if (m_inputField) m_inputField->setText(m_lab.attackInput);
    runWithInput(m_lab.attackInput);
}

// ── Core execution ────────────────────────────────────────────────────────────

void SecurityLabWidget::runWithInput(const QString& input)
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }

    m_outputPane->clear();
    m_pendingInput = input;

    bool showingFix = (m_codeStack->currentIndex() == 1);
    QString program = showingFix ? m_lab.labProgram2 : m_lab.labProgram;

    if (m_lab.isPython) {
        // Run with python
        if (!m_process) {
            m_process = new QProcess(this);
            connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
                appendOutput(QString::fromLocal8Bit(m_process->readAllStandardOutput()));
            });
            connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
                appendOutput(QString::fromLocal8Bit(m_process->readAllStandardError()), "#f38ba8");
            });
            connect(m_process,
                    QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, &SecurityLabWidget::onProcessFinished);
            connect(m_process, &QProcess::errorOccurred,
                    this, &SecurityLabWidget::onProcessError);
        }

        // Find python executable
        QString pythonExe;
        for (const QString& candidate : {"python3", "python", "python3.exe", "python.exe"}) {
            if (QProcess::execute(candidate, {"--version"}) == 0) {
                pythonExe = candidate;
                break;
            }
        }
        if (pythonExe.isEmpty()) pythonExe = "python";

        m_statusLabel->setText("Running...");
        m_runNormalBtn->setEnabled(false);
        m_runAttackBtn->setEnabled(false);

        m_process->start(pythonExe, {program});
        m_process->write((input + "\n").toLocal8Bit());
        m_process->closeWriteChannel();

    } else {
        // C program — on Windows run the .exe; compile first if not present
        QString exePath = program;
#ifdef Q_OS_WIN
        if (!exePath.endsWith(".exe")) exePath += ".exe";
#endif
        if (!QFileInfo::exists(exePath)) {
            // Try to compile with gcc
            QString srcPath = program + ".c";
#ifdef Q_OS_WIN
            srcPath = program.chopped(0) + ".c"; // program without .exe
            if (program.endsWith(".exe"))
                srcPath = program.left(program.length()-4) + ".c";
            else
                srcPath = program + ".c";
#endif
            appendOutput("Compiling " + QFileInfo(srcPath).fileName() + "...\n", "#f9e2af");
            QProcess compiler;
            compiler.start("gcc", {srcPath, "-o", exePath, "-w"});
            compiler.waitForFinished(10000);
            if (compiler.exitCode() != 0) {
                appendOutput("Compilation failed:\n", "#f38ba8");
                appendOutput(QString::fromLocal8Bit(compiler.readAllStandardError()), "#f38ba8");
                m_statusLabel->setText("Compilation failed.");
                return;
            }
            appendOutput("Compiled OK.\n\n", "#a6e3a1");
        }

        if (!m_process) {
            m_process = new QProcess(this);
            connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
                appendOutput(QString::fromLocal8Bit(m_process->readAllStandardOutput()));
            });
            connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
                appendOutput(QString::fromLocal8Bit(m_process->readAllStandardError()), "#f38ba8");
            });
            connect(m_process,
                    QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, &SecurityLabWidget::onProcessFinished);
            connect(m_process, &QProcess::errorOccurred,
                    this, &SecurityLabWidget::onProcessError);
        }

        m_statusLabel->setText("Running...");
        m_runNormalBtn->setEnabled(false);
        m_runAttackBtn->setEnabled(false);

        m_process->start(exePath, {});
        m_process->write((input + "\n").toLocal8Bit());
        m_process->closeWriteChannel();
    }

    // Safety timeout — kill after 10 seconds
    QTimer::singleShot(10000, this, [this]() {
        if (m_process && m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            appendOutput("\n[Process killed — exceeded 10 second timeout]\n", "#f38ba8");
        }
    });
}

void SecurityLabWidget::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    m_runNormalBtn->setEnabled(true);
    m_runAttackBtn->setEnabled(true);

    if (status == QProcess::CrashExit) {
        appendOutput("\n[Process crashed or was killed by OS]\n", "#f38ba8");
        m_statusLabel->setText("Process crashed.");
    } else if (exitCode != 0) {
        appendOutput(QString("\n[Process exited with code %1]\n").arg(exitCode), "#f9e2af");
        m_statusLabel->setText(QString("Exited: code %1").arg(exitCode));
    } else {
        m_statusLabel->setText("Done.");
    }
}

void SecurityLabWidget::onProcessError(QProcess::ProcessError err)
{
    m_runNormalBtn->setEnabled(true);
    m_runAttackBtn->setEnabled(true);

    QString msg;
    switch (err) {
    case QProcess::FailedToStart:
        msg = "Failed to start program. Is Python installed?";
        break;
    case QProcess::Crashed:
        msg = "Process crashed.";
        break;
    default:
        msg = "Process error.";
        break;
    }
    appendOutput("\n[ERROR] " + msg + "\n", "#f38ba8");
    m_statusLabel->setText(msg);
}

// ── Show Fix ──────────────────────────────────────────────────────────────────

void SecurityLabWidget::showFix()
{
    m_showingFix = !m_showingFix;
    m_codeStack->setCurrentIndex(m_showingFix ? 1 : 0);

    if (m_showingFix) {
        m_fixBtn->setText("Show Vulnerable Code");
        m_fixBtn->setStyleSheet(btnStyle("#a6e3a1", "#c3f5bf"));
        appendOutput("\n--- FIXED VERSION ---\n", "#a6e3a1");
        appendOutput(m_lab.fixExplanation + "\n", "#a6e3a1");
    } else {
        m_fixBtn->setText("Show the Fix");
        m_fixBtn->setStyleSheet(btnStyle("#89b4fa", "#b4d0fb"));
        appendOutput("\n--- VULNERABLE VERSION ---\n", "#f38ba8");
    }
}

// ── Why This Matters ──────────────────────────────────────────────────────────

void SecurityLabWidget::showWhyItMatters()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Why This Matters");
    dlg->setModal(true);
    dlg->setMinimumWidth(460);
    dlg->setStyleSheet("background: #1e1e2e;");

    auto* lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(22, 18, 22, 18);
    lay->setSpacing(14);

    auto* title = new QLabel(m_lab.name);
    title->setStyleSheet("color: #f38ba8; font-size: 16px; font-weight: bold;");
    lay->addWidget(title);

    auto* body = new QLabel(m_lab.explanation);
    body->setWordWrap(true);
    body->setStyleSheet("color: #cdd6f4; font-size: 13px; line-height: 150%;");
    lay->addWidget(body);

    auto* closeBtn = new QPushButton("Close");
    closeBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 6px; padding: 0 18px; font-size: 13px; min-height: 30px; }"
        "QPushButton:hover { background: #45475a; }");
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    lay->addLayout(btnRow);

    dlg->exec();
    dlg->deleteLater();
}

// ── Output helper ─────────────────────────────────────────────────────────────

void SecurityLabWidget::appendOutput(const QString& text, const QString& color)
{
    if (color == "#cdd6f4") {
        m_outputPane->moveCursor(QTextCursor::End);
        m_outputPane->insertPlainText(text);
    } else {
        QString html = QString("<span style=\"color:%1;\">%2</span>")
            .arg(color, text.toHtmlEscaped().replace("\n", "<br>"));
        m_outputPane->moveCursor(QTextCursor::End);
        m_outputPane->insertHtml(html);
    }
    m_outputPane->moveCursor(QTextCursor::End);
}
