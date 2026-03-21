#include "core/dependencyanalysis.h"
#include "core/jsonloader.h"
#include "core/theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTextStream>
#include <QMessageBox>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>

// ─────────────────────────────────────────────────────────────────────────────
// Known package descriptions — loaded from data/packages.json
// ─────────────────────────────────────────────────────────────────────────────
QMap<QString, QString> DependencyAnalysisFrame::knownDescriptions()
{
    static QMap<QString, QString> db;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        QJsonObject obj = JsonLoader::loadObject("packages.json");
        QJsonObject descs = obj["descriptions"].toObject();
        for (auto it = descs.begin(); it != descs.end(); ++it)
            db[it.key()] = it.value().toString();
    }
    return db;
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
DependencyAnalysisFrame::DependencyAnalysisFrame(QWidget* parent)
    : QWidget(parent)
{
    buildUI();
}

void DependencyAnalysisFrame::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header bar ────────────────────────────────────────────────────────
    auto* headerBar = new QWidget(this);
    headerBar->setFixedHeight(48);
    headerBar->setStyleSheet("background: #2a2a3c; border-bottom: 1px solid #313244;");
    auto* hdr = new QHBoxLayout(headerBar);
    hdr->setContentsMargins(16, 0, 16, 0);

    auto* title = new QLabel("Dependency Analysis", headerBar);
    title->setStyleSheet(
        "QLabel { color: #cdd6f4; font-size: 15px; font-weight: bold; background: transparent; }");
    hdr->addWidget(title);
    hdr->addStretch();

    m_helpBtn = new QPushButton("?", headerBar);
    m_helpBtn->setFixedSize(28, 28);
    m_helpBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #a6adc8; border-radius: 14px;"
        " font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: #45475a; color: #cdd6f4; }");
    connect(m_helpBtn, &QPushButton::clicked, this, &DependencyAnalysisFrame::onHelpClicked);
    hdr->addWidget(m_helpBtn);

    hdr->addSpacing(8);

    m_backBtn = new QPushButton("Back to Editor", headerBar);
    m_backBtn->setStyleSheet(
        "QPushButton { background: #313244; color: #a6adc8; border-radius: 6px;"
        " padding: 6px 16px; font-size: 12px; }"
        "QPushButton:hover { background: #45475a; color: #cdd6f4; }");
    connect(m_backBtn, &QPushButton::clicked, this, &DependencyAnalysisFrame::backToEditor);
    hdr->addWidget(m_backBtn);

    root->addWidget(headerBar);

    // ── Toolbar row (scan button + status) ────────────────────────────────
    auto* toolBar = new QWidget(this);
    toolBar->setStyleSheet("QWidget { background: #1e1e2e; }");
    auto* tbLayout = new QHBoxLayout(toolBar);
    tbLayout->setContentsMargins(32, 16, 32, 12);
    tbLayout->setSpacing(16);

    m_scanBtn = new QPushButton("Scan Dependencies", toolBar);
    m_scanBtn->setStyleSheet(
        "QPushButton { background: #89b4fa; color: #1e1e2e; border-radius: 6px;"
        " padding: 7px 20px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background: #74c7ec; }"
        "QPushButton:disabled { background: #313244; color: #45475a; }");
    connect(m_scanBtn, &QPushButton::clicked, this, &DependencyAnalysisFrame::onScan);
    tbLayout->addWidget(m_scanBtn, 0, Qt::AlignLeft);

    m_statusLabel = new QLabel("No scan performed yet.", toolBar);
    m_statusLabel->setStyleSheet(
        "QLabel { color: #6c7086; font-size: 11px; background: transparent; }");
    tbLayout->addWidget(m_statusLabel, 1);

    root->addWidget(toolBar);

    // ── Scrollable cards area ─────────────────────────────────────────────
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setStyleSheet("QScrollArea { background: #1e1e2e; border: none; }");

    m_cardsWidget = new QWidget;
    m_cardsWidget->setStyleSheet("QWidget { background: #1e1e2e; }");
    auto* cardsLayout = new QVBoxLayout(m_cardsWidget);
    cardsLayout->setContentsMargins(32, 8, 32, 32);
    cardsLayout->setSpacing(0);
    cardsLayout->addStretch();

    m_scroll->setWidget(m_cardsWidget);
    root->addWidget(m_scroll, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────
void DependencyAnalysisFrame::setCode(const QString& code,
                                      const QString& language,
                                      const QString& filePath)
{
    m_code     = code;
    m_language = language.toLower();
    m_filePath = filePath;
    clearResults();
    m_statusLabel->setText("Click 'Scan Dependencies' to begin.");
}

// ─────────────────────────────────────────────────────────────────────────────
// Scan slot
// ─────────────────────────────────────────────────────────────────────────────
void DependencyAnalysisFrame::onScan()
{
    clearResults();
    m_scanBtn->setEnabled(false);
    m_statusLabel->setText("Scanning...");

    QStringList names;
    QList<DepInfo> deps;

    if (m_language == "python") {
        names = parsePythonImports(m_code);
        deps  = checkPythonDeps(names);
    } else if (m_language == "c" || m_language == "cpp" || m_language == "c++") {
        names = parseCIncludes(m_code);
        deps  = checkCDeps(names);
    } else if (m_language == "javascript" || m_language == "js") {
        names = parseJSImports(m_code);
        deps  = checkJSDeps(names);
    } else if (m_language == "rust") {
        names = parseRustUses(m_code);
        deps  = checkRustDeps(names);
    } else {
        m_statusLabel->setText(
            QString("Dependency scanning not supported for '%1'.").arg(m_language));
        m_scanBtn->setEnabled(true);
        return;
    }

    showResults(deps);
    m_scanBtn->setEnabled(true);

    if (deps.isEmpty()) {
        m_statusLabel->setText("No dependencies found.");
    } else {
        int ok  = 0, warn = 0, bad = 0;
        for (auto& d : deps) {
            if      (d.vulnerable) bad++;
            else if (!d.installed) warn++;
            else                   ok++;
        }
        m_statusLabel->setText(
            QString("%1 found — %2 OK, %3 not installed, %4 vulnerable")
                .arg(deps.count()).arg(ok).arg(warn).arg(bad));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Parsing helpers
// ─────────────────────────────────────────────────────────────────────────────
QStringList DependencyAnalysisFrame::parsePythonImports(const QString& code) const
{
    QStringList result;
    QRegularExpression reImport(
        R"(^\s*import\s+([\w,\s]+)|^\s*from\s+(\w+)\s+import)",
        QRegularExpression::MultilineOption);

    auto it = reImport.globalMatch(code);
    while (it.hasNext()) {
        auto m = it.next();
        QString grp1 = m.captured(1).trimmed();
        QString grp2 = m.captured(2).trimmed();
        if (!grp1.isEmpty()) {
            for (const QString& part : grp1.split(',')) {
                QString name = part.trimmed().split(' ').first();
                if (!name.isEmpty() && !result.contains(name))
                    result << name;
            }
        }
        if (!grp2.isEmpty() && !result.contains(grp2))
            result << grp2;
    }
    return result;
}

QStringList DependencyAnalysisFrame::parseCIncludes(const QString& code) const
{
    QStringList result;
    QRegularExpression re(R"(^\s*#\s*include\s*[<"]([\w./]+)[>"])",
                          QRegularExpression::MultilineOption);
    auto it = re.globalMatch(code);
    while (it.hasNext()) {
        auto m = it.next();
        QString name = m.captured(1).trimmed();
        if (!name.isEmpty() && !result.contains(name))
            result << name;
    }
    return result;
}

QStringList DependencyAnalysisFrame::parseJSImports(const QString& code) const
{
    QStringList result;
    // require("x") / require('x')
    QRegularExpression reReq(R"(require\s*\(\s*['"]([^'"./][^'"]*)['"]\s*\))");
    // import ... from "x"
    QRegularExpression reImport(R"(import\s+.*?from\s+['"]([^'"./][^'"]*)['"]\s*;?)");

    for (auto* re : {&reReq, &reImport}) {
        auto it = re->globalMatch(code);
        while (it.hasNext()) {
            QString name = it.next().captured(1).trimmed();
            if (!name.isEmpty() && !result.contains(name))
                result << name;
        }
    }
    return result;
}

QStringList DependencyAnalysisFrame::parseRustUses(const QString& code) const
{
    QStringList result;
    // extern crate x;
    QRegularExpression reExtern(R"(extern\s+crate\s+(\w+)\s*;)");
    // use x::  (top-level crate uses)
    QRegularExpression reUse(R"(^\s*use\s+(\w+)::)", QRegularExpression::MultilineOption);

    for (auto* re : {&reExtern, &reUse}) {
        auto it = re->globalMatch(code);
        while (it.hasNext()) {
            QString name = it.next().captured(1).trimmed();
            if (!name.isEmpty() && !result.contains(name))
                result << name;
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Dependency checking
// ─────────────────────────────────────────────────────────────────────────────
QList<DepInfo> DependencyAnalysisFrame::checkPythonDeps(const QStringList& names)
{
    auto desc = knownDescriptions();
    QList<DepInfo> deps;

    for (const QString& name : names) {
        DepInfo d;
        d.name        = name;
        d.description = desc.value(name, "(Third-party library)");

        // Standard library check (common stdlib modules)
        static const QStringList stdlib = {
            "os","sys","re","math","json","time","datetime","collections",
            "itertools","functools","pathlib","typing","io","copy","hashlib",
            "threading","multiprocessing","subprocess","socket","http","urllib",
            "email","html","xml","csv","sqlite3","unittest","logging","argparse",
            "enum","dataclasses","abc","contextlib","shutil","tempfile","glob",
            "struct","ctypes","gc","weakref","inspect","ast","tokenize","dis",
            "traceback","warnings","random","statistics","decimal","fractions",
            "heapq","bisect","array","queue","string","textwrap","difflib",
            "pprint","reprlib","operator","types","numbers","cmath","base64",
            "binascii","zlib","gzip","bz2","lzma","zipfile","tarfile","pickle",
            "shelve","dbm","configparser","platform","signal","mmap","errno"
        };

        if (stdlib.contains(name)) {
            d.installed   = true;
            d.version     = "stdlib";
            d.description = "(Python standard library module)";
            d.vulnerable  = false;
            deps << d;
            continue;
        }

        // Ask pip show
        QProcess pip;
        pip.start("pip", {"show", name});
        pip.waitForFinished(5000);
        QString out = pip.readAllStandardOutput();

        if (!out.isEmpty()) {
            d.installed = true;
            QRegularExpression verRe("Version:\\s*([^\\n]+)");
            auto vm = verRe.match(out);
            if (vm.hasMatch()) d.version = vm.captured(1).trimmed();
            d.vulnerable = false;
        } else {
            d.installed = false;
            d.version   = "";
            d.vulnerable = false;
        }
        deps << d;
    }
    return deps;
}

QList<DepInfo> DependencyAnalysisFrame::checkCDeps(const QStringList& names)
{
    auto desc = knownDescriptions();
    QList<DepInfo> deps;

    // Known system headers that are always "installed"
    static const QStringList systemHeaders = {
        "stdio.h","stdlib.h","string.h","math.h","time.h","errno.h",
        "assert.h","ctype.h","limits.h","float.h","signal.h","setjmp.h",
        "stdarg.h","stddef.h","stdint.h","stdbool.h","inttypes.h",
        "locale.h","wchar.h","wctype.h","iso646.h",
        "pthread.h","unistd.h","fcntl.h","sys/types.h","sys/stat.h",
        "sys/socket.h","netinet/in.h","arpa/inet.h","netdb.h",
        "windows.h","winsock2.h","ws2tcpip.h","psapi.h","dwmapi.h"
    };

    for (const QString& name : names) {
        DepInfo d;
        d.name        = name;
        d.description = desc.value(name, "(C/C++ header)");
        d.version     = "system";
        d.installed   = systemHeaders.contains(name);
        d.vulnerable  = false;
        deps << d;
    }
    return deps;
}

QList<DepInfo> DependencyAnalysisFrame::checkJSDeps(const QStringList& names)
{
    auto desc = knownDescriptions();
    QList<DepInfo> deps;

    for (const QString& name : names) {
        DepInfo d;
        d.name        = name;
        d.description = desc.value(name, "(npm package)");

        // npm list --depth=0 --json is slow; just check node_modules dir
        // relative to file path
        QString nmPath;
        if (!m_filePath.isEmpty()) {
            QFileInfo fi(m_filePath);
            nmPath = fi.absoluteDir().absolutePath()
                     + "/node_modules/" + name;
        }

        if (!nmPath.isEmpty() && QFile::exists(nmPath)) {
            d.installed = true;
            // Try to read package.json for version
            QString pkgJson = nmPath + "/package.json";
            QFile f(pkgJson);
            if (f.open(QFile::ReadOnly)) {
                QString content = f.readAll();
                QRegularExpression verRe("\"version\"\\s*:\\s*\"([^\"]+)\"");
                auto vm = verRe.match(content);
                if (vm.hasMatch()) d.version = vm.captured(1);
            }
        } else {
            d.installed = false;
        }
        d.vulnerable = false;
        deps << d;
    }
    return deps;
}

QList<DepInfo> DependencyAnalysisFrame::checkRustDeps(const QStringList& names)
{
    auto desc = knownDescriptions();
    QList<DepInfo> deps;

    // Rust built-ins that are always available
    static const QStringList builtins = {
        "std","core","alloc","proc_macro","test","crate","self","super"
    };

    for (const QString& name : names) {
        DepInfo d;
        d.name        = name;
        d.description = desc.value(name, "(Rust crate)");

        if (builtins.contains(name)) {
            d.installed   = true;
            d.version     = "built-in";
            d.description = "(Rust built-in)";
            d.vulnerable  = false;
        } else {
            // Check Cargo.lock or Cargo.toml for version — simplified
            QString cargoLockPath;
            if (!m_filePath.isEmpty()) {
                QFileInfo fi(m_filePath);
                cargoLockPath = fi.absoluteDir().absolutePath() + "/Cargo.lock";
            }
            d.installed = false;
            d.version   = "";
            if (!cargoLockPath.isEmpty() && QFile::exists(cargoLockPath)) {
                QFile f(cargoLockPath);
                if (f.open(QFile::ReadOnly)) {
                    QString content = f.readAll();
                    QRegularExpression re(
                        QString("name\\s*=\\s*\"%1\"\\s*\\nversion\\s*=\\s*\"([^\"]+)\"")
                            .arg(name));
                    auto vm = re.match(content);
                    if (vm.hasMatch()) {
                        d.installed = true;
                        d.version   = vm.captured(1);
                    }
                }
            }
            d.vulnerable = false;
        }
        deps << d;
    }
    return deps;
}

// ─────────────────────────────────────────────────────────────────────────────
// Results display
// ─────────────────────────────────────────────────────────────────────────────
void DependencyAnalysisFrame::clearResults()
{
    auto* layout = qobject_cast<QVBoxLayout*>(m_cardsWidget->layout());
    if (!layout) return;

    // Remove all children except the stretch at the end
    while (layout->count() > 1) {
        auto* item = layout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

void DependencyAnalysisFrame::showResults(const QList<DepInfo>& deps)
{
    clearResults();
    auto* layout = qobject_cast<QVBoxLayout*>(m_cardsWidget->layout());
    if (!layout) return;

    for (const DepInfo& dep : deps) {
        QWidget* card = makeCard(dep);
        layout->insertWidget(layout->count() - 1, card);
    }
}

QWidget* DependencyAnalysisFrame::makeCard(const DepInfo& dep)
{
    QString statusColor, statusText;
    if (dep.vulnerable) {
        statusColor = "#f38ba8";
        statusText  = "Vulnerable";
    } else if (!dep.installed) {
        statusColor = "#f9e2af";
        statusText  = "Not installed";
    } else {
        statusColor = "#a6e3a1";
        statusText  = "OK";
    }

    auto* card = new QWidget;
    card->setStyleSheet(
        "QWidget#depCard { background: #2a2a3c; border-radius: 8px;"
        " border-left: 3px solid " + statusColor + "; }"
        "QWidget { background: transparent; }");
    card->setObjectName("depCard");

    auto* cl = new QVBoxLayout(card);
    cl->setContentsMargins(16, 12, 16, 12);
    cl->setSpacing(4);

    // Top row: name | version | status dot
    auto* topRow = new QHBoxLayout;
    topRow->setSpacing(12);

    auto* nameLabel = new QLabel(dep.name, card);
    nameLabel->setStyleSheet(
        QString("QLabel { color: #cdd6f4; font-size: 13px; font-weight: bold;"
                " font-family: 'Cascadia Code'; background: transparent; }"));
    topRow->addWidget(nameLabel);

    if (!dep.version.isEmpty()) {
        auto* verLabel = new QLabel(dep.version, card);
        verLabel->setStyleSheet(
            "QLabel { color: #a6adc8; font-size: 11px; background: transparent;"
            " font-family: 'Cascadia Code'; }");
        topRow->addWidget(verLabel);
    }

    topRow->addStretch();

    auto* statusBadge = new QLabel(statusText, card);
    statusBadge->setStyleSheet(
        QString("QLabel { color: %1; font-size: 11px; font-weight: bold;"
                " background: transparent; }").arg(statusColor));
    topRow->addWidget(statusBadge);

    cl->addLayout(topRow);

    // Description
    if (!dep.description.isEmpty()) {
        auto* descLabel = new QLabel(dep.description, card);
        descLabel->setStyleSheet(
            "QLabel { color: #6c7086; font-size: 11px; background: transparent; }");
        descLabel->setWordWrap(true);
        cl->addWidget(descLabel);
    }

    // Bottom spacing
    auto* spacer = new QWidget;
    spacer->setFixedHeight(8);
    cl->addWidget(spacer);

    return card;
}

// ─────────────────────────────────────────────────────────────────────────────
// Help
// ─────────────────────────────────────────────────────────────────────────────
void DependencyAnalysisFrame::onHelpClicked()
{
    QMessageBox::information(this, "Dependency Analysis Help",
        "Dependency Analysis scans your source file for imported packages/headers/modules.\n\n"
        "Python: scans 'import' and 'from ... import' statements.\n"
        "  Checks each package via 'pip show'. Standard library modules are always OK.\n\n"
        "C/C++: scans #include lines.\n"
        "  Common system headers are marked installed. Third-party headers are flagged.\n\n"
        "JavaScript: scans require() and import ... from statements.\n"
        "  Checks for node_modules/<name> in the same directory as your file.\n\n"
        "Rust: scans 'use' and 'extern crate' statements.\n"
        "  Checks Cargo.lock in the same directory.\n\n"
        "Status colours:\n"
        "  Green — installed and OK\n"
        "  Yellow — not installed / not found\n"
        "  Red — known vulnerability detected");
}
