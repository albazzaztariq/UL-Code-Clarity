#include "core/optimizer.h"

#include <QRegularExpression>
#include <QSet>

// ============================================================================
// Public entry point
// ============================================================================
QList<OptimizationEntry> CodeOptimizer::analyzeFile(const QString& code,
                                                     const QString& language,
                                                     int level) const
{
    QStringList lines = code.split('\n');

    if (language == "python")
        return analyzePython(lines, level);
    if (language == "c" || language == "cpp")
        return analyzeCpp(lines, language == "cpp", level);

    return {};
}

// ============================================================================
// Cross-language note
// ============================================================================
QString CodeOptimizer::crossLanguageNote(const QString& fasterLang,
                                          double speedRatio,
                                          int level)
{
    QString ratio = QString::number(speedRatio, 'f', 1);
    QString fasterUpper = fasterLang.toUpper();

    if (level <= 2) {
        return QString(
            "Python ran %1x slower than the %2 version.\n\n"
            "That's not a bug \xe2\x80\x94 it's how these languages work. Python reads your "
            "code line by line every time it runs, which takes time. %2 is translated into "
            "direct CPU instructions before it runs, so it starts fast and stays fast.\n\n"
            "Think of it like this: Python is like reading a recipe out loud while you cook. "
            "%2 is like having the meal already prepared. For learning and quick experiments, "
            "Python is great. For programs that need to run many times or handle a lot of "
            "data quickly, a compiled language like C or Rust is the right choice."
        ).arg(ratio, fasterUpper);
    }

    return QString(
        "Python ran %1x slower than the %2 version.\n\n"
        "Python executes via CPython bytecode interpreted at runtime; %2 compiles to "
        "native machine code with compiler optimisations (-O2 here). The overhead is "
        "inherent to the dynamic dispatch, GIL, and reference counting in CPython.\n\n"
        "For this workload, consider C, Rust, or UL (compiled to C) for production. "
        "Use Python for prototyping, scripting, and tasks where development speed matters "
        "more than execution speed."
    ).arg(ratio, fasterUpper);
}

// ============================================================================
// Helper: is 'name' used anywhere after line defLine (0-based)?
// ============================================================================
bool CodeOptimizer::nameUsedAfterLine(const QStringList& lines,
                                      const QString& name,
                                      int defLine) const
{
    QRegularExpression re(QString("\\b%1\\b").arg(QRegularExpression::escape(name)));
    for (int i = defLine + 1; i < lines.size(); ++i) {
        if (re.match(lines[i]).hasMatch())
            return true;
    }
    return false;
}

// ============================================================================
// Python analysis
// ============================================================================
QList<OptimizationEntry> CodeOptimizer::analyzePython(const QStringList& lines,
                                                       int level) const
{
    QList<OptimizationEntry> results;

    // ── 1. Unused imports ────────────────────────────────────────────────
    QRegularExpression importRe(R"(^\s*import\s+(\w+)(?:\s+as\s+(\w+))?\s*$)");
    QRegularExpression fromImportRe(R"(^\s*from\s+\S+\s+import\s+(\w+)(?:\s+as\s+(\w+))?\s*$)");

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];

        auto m = importRe.match(line);
        QString used;
        if (m.hasMatch()) {
            used = m.captured(2).isEmpty() ? m.captured(1) : m.captured(2);
        } else {
            auto m2 = fromImportRe.match(line);
            if (!m2.hasMatch()) continue;
            used = m2.captured(2).isEmpty() ? m2.captured(1) : m2.captured(2);
        }

        if (!nameUsedAfterLine(lines, used, i)) {
            OptimizationEntry e;
            e.title       = "Unused import";
            e.description = QString("'%1' is imported but never used anywhere in the file.")
                .arg(used);
            if (level <= 2) {
                e.whyItMatters = QString(
                    "Every time Python starts your program, it runs every import statement "
                    "at the top of your file. Each import loads a module from disk, parses "
                    "it, and sets up its contents in memory \xe2\x80\x94 even if you never use it.\n\n"
                    "Removing unused imports makes your program start faster, use less memory, "
                    "and is less confusing for anyone reading the code (including your future self)."
                );
            } else {
                e.whyItMatters = QString(
                    "Python executes import statements at module load time. Even unused imports "
                    "incur the cost of disk I/O, bytecode compilation (or .pyc cache lookup), "
                    "and insertion into sys.modules. On a cold start with many unused imports this "
                    "compounds. Linters (ruff, pylint) flag this as F401."
                );
            }
            e.originalCode  = line.trimmed();
            e.suggestedCode = QString("# Remove this line \xe2\x80\x94 %1 is never used:\n"
                                      "# %2").arg(used, line.trimmed());
            e.lineNumber    = i + 1;
            results.append(e);
        }
    }

    // ── 2. String concatenation in loop ─────────────────────────────────
    QRegularExpression loopRe(R"(^\s*(for|while)\s)");
    QRegularExpression strConcatRe(R"((\w+)\s*\+=\s*[\"'])");
    bool inLoop   = false;
    int  loopIndent = 0;

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        if (loopRe.match(line).hasMatch()) {
            inLoop = true;
            int indent = 0;
            while (indent < line.size() && line[indent] == ' ') ++indent;
            loopIndent = indent;
        } else if (inLoop) {
            int indent = 0;
            while (indent < line.size() && line[indent] == ' ') ++indent;
            if (!line.trimmed().isEmpty() && indent <= loopIndent)
                inLoop = false;
        }

        if (inLoop) {
            auto m = strConcatRe.match(line);
            if (m.hasMatch()) {
                QString varName = m.captured(1);
                OptimizationEntry e;
                e.title       = "String concatenation in loop";
                e.description = QString(
                    "Using '%1 +=' inside a loop is slow because strings in Python "
                    "are immutable \xe2\x80\x94 every '+=' creates a brand-new string.").arg(varName);
                if (level <= 2) {
                    e.whyItMatters = QString(
                        "Imagine you're writing a book by rewriting the entire thing from "
                        "scratch every time you add a new sentence. That's what '%1 +=' does "
                        "inside a loop \xe2\x80\x94 Python can't add to the end of an existing string, "
                        "so it creates a completely new one and copies everything over.\n\n"
                        "With 100 loop iterations, you make 100 copies. With 10,000 iterations, "
                        "you make 10,000 copies. Each copy gets bigger than the last, so the "
                        "total work grows as n\xc2\xb2 (quadratic).\n\n"
                        "The fix: collect pieces in a list, then join them once at the end. "
                        "A list can grow cheaply. ''.join(parts) does one single pass."
                    ).arg(varName);
                } else {
                    e.whyItMatters = QString(
                        "CPython strings are immutable; '%1 +=' allocates a new str object "
                        "of size n+k on each iteration (where n is the current length and k is "
                        "the appended chunk). Total allocations: O(n\xc2\xb2). Using a list and "
                        "str.join() amortises the cost: list.append() is O(1) amortised, and "
                        "str.join() makes a single pass with one allocation."
                    ).arg(varName);
                }
                e.originalCode  = line.trimmed();
                e.suggestedCode = QString(
                    "# Before the loop:\n"
                    "parts = []\n\n"
                    "# Inside the loop, replace '%1 += ...' with:\n"
                    "parts.append(...)  # cheap list append\n\n"
                    "# After the loop:\n"
                    "%1 = ''.join(parts)  # one efficient string build"
                ).arg(varName);
                e.lineNumber = i + 1;
                results.append(e);
                inLoop = false;
            }
        }
    }

    // ── 3. List used for 'in' membership check ───────────────────────────
    QRegularExpression listAssignRe(R"(^\s*(\w+)\s*=\s*\[)");
    QRegularExpression memberCheckRe(R"(\bif\s+\w+\s+in\s+(\w+))");
    QSet<QString> listVars;

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        auto m = listAssignRe.match(line);
        if (m.hasMatch())
            listVars.insert(m.captured(1));

        auto m2 = memberCheckRe.match(line);
        if (m2.hasMatch() && listVars.contains(m2.captured(1))) {
            QString varName = m2.captured(1);
            OptimizationEntry e;
            e.title       = "Use a set instead of a list for membership tests";
            e.description = QString(
                "'if x in %1' checks every item in the list one by one to find a match.")
                .arg(varName);
            if (level <= 2) {
                e.whyItMatters = QString(
                    "When you write 'if x in %1' and %1 is a list, Python has to look "
                    "at each item from the beginning until it finds x (or reaches the end). "
                    "If your list has 1,000 items, that's up to 1,000 comparisons for each check.\n\n"
                    "A set works completely differently. It uses a hash table, which can jump "
                    "directly to the right spot in memory \xe2\x80\x94 like having an index at the back "
                    "of a book instead of reading every page. Converting the list to a set takes "
                    "a moment, but after that every 'in' check is near-instant regardless of size."
                ).arg(varName);
            } else {
                e.whyItMatters = QString(
                    "list.__contains__ is O(n) \xe2\x80\x94 it does a linear scan. set.__contains__ "
                    "is O(1) average via hash table lookup. If this check is inside a loop, "
                    "the overall complexity drops from O(n\xc2\xb2) to O(n). Convert %1 to a set "
                    "once before the loop; the one-time O(n) cost of set() is amortised away."
                ).arg(varName);
            }
            e.originalCode  = line.trimmed();
            e.suggestedCode = QString(
                "# Convert once, before any loop that checks membership:\n"
                "%1 = set(%1)\n\n"
                "# Then your existing check works unchanged and is now O(1):\n"
                "%2"
            ).arg(varName, line.trimmed());
            e.lineNumber = i + 1;
            results.append(e);
            listVars.remove(varName);
        }
    }

    // ── 4. O(n²) nested loops over same collection ───────────────────────
    QRegularExpression forIterRe(R"(^\s*for\s+\w+\s+in\s+(\w+)\s*:)");

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        auto m = forIterRe.match(line);
        if (!m.hasMatch()) continue;

        QString collection = m.captured(1);
        int outerIndent = 0;
        while (outerIndent < line.size() && line[outerIndent] == ' ') ++outerIndent;

        for (int j = i + 1; j < lines.size() && j < i + 40; ++j) {
            const QString& inner = lines[j];
            if (inner.trimmed().isEmpty()) continue;
            int innerIndent = 0;
            while (innerIndent < inner.size() && inner[innerIndent] == ' ') ++innerIndent;
            if (innerIndent <= outerIndent) break;

            auto m2 = forIterRe.match(inner);
            if (m2.hasMatch() && m2.captured(1) == collection) {
                OptimizationEntry e;
                e.title       = "O(n\xc2\xb2) nested loop over the same collection";
                e.description = QString(
                    "Two nested loops both iterating over '%1' means the inner loop "
                    "runs once for every item in the outer loop.").arg(collection);
                if (level <= 2) {
                    e.whyItMatters = QString(
                        "If '%1' has 100 items, the inner loop runs 100 \xc3\x97 100 = 10,000 times. "
                        "With 1,000 items it runs 1,000,000 times. The work grows as the square "
                        "of the input size \xe2\x80\x94 this is called O(n\xc2\xb2) (\"order n-squared\").\n\n"
                        "A few alternatives depending on what you're doing:\n"
                        "\xe2\x80\xa2 Finding pairs: itertools.combinations(%1, 2) is cleaner and faster.\n"
                        "\xe2\x80\xa2 Looking something up: use a dictionary so each lookup is instant.\n"
                        "\xe2\x80\xa2 Comparing all vs all: sometimes unavoidable, but sort first and "
                        "use two-pointer technique to reduce work."
                    ).arg(collection);
                } else {
                    e.whyItMatters = QString(
                        "Nested iteration over the same collection is O(n\xc2\xb2). Depending on the "
                        "goal: use itertools.combinations for pair enumeration (same complexity "
                        "but Pythonic), a dict/set for O(1) lookup, or sort + two-pointer for "
                        "two-sum style problems (O(n log n))."
                    ).arg(collection);
                }
                e.originalCode  = line.trimmed() + "\n    " + inner.trimmed();
                e.suggestedCode =
                    "# Option 1 \xe2\x80\x94 if you need all pairs:\n"
                    "import itertools\n"
                    "for a, b in itertools.combinations(" + collection + ", 2):\n"
                    "    ...\n\n"
                    "# Option 2 \xe2\x80\x94 if you're searching for a match, use a set/dict:\n"
                    "lookup = set(" + collection + ")\n"
                    "for item in " + collection + ":\n"
                    "    if target in lookup:  # O(1) instead of O(n)\n"
                    "        ...";
                e.lineNumber = i + 1;
                results.append(e);
                break;
            }
        }
    }

    // ── 5. Missing type hints on function params ─────────────────────────
    QRegularExpression defRe(R"(^\s*def\s+(\w+)\s*\(([^)]*)\)\s*:)");
    for (int i = 0; i < lines.size(); ++i) {
        auto m = defRe.match(lines[i]);
        if (!m.hasMatch()) continue;
        QString params = m.captured(2).trimmed();
        if (params.isEmpty() || params == "self" || params == "cls") continue;
        QStringList paramList = params.split(',');
        bool missingHints = false;
        for (const QString& p : paramList) {
            QString pt = p.trimmed();
            if (pt == "self" || pt == "cls" || pt.startsWith("*") || pt.isEmpty()) continue;
            if (!pt.contains(':') && !pt.contains('=')) { missingHints = true; break; }
        }
        if (missingHints) {
            OptimizationEntry e;
            e.title       = "Missing type hints";
            e.description = QString(
                "Function '%1' has parameters with no type annotations.").arg(m.captured(1));
            if (level <= 2) {
                e.whyItMatters = QString(
                    "Type hints are notes you add to your function that say what kind of "
                    "data each parameter expects (a number, a string, a list, etc.).\n\n"
                    "They don't make the program faster by themselves, but they let tools "
                    "like mypy check your code before you run it and catch mistakes early \xe2\x80\x94 "
                    "like passing a string to a function that expects a number. They also make "
                    "your code much easier to understand at a glance.\n\n"
                    "Example: 'def add(a: int, b: int) -> int:' tells you and your tools "
                    "that add() takes two integers and returns an integer."
                );
            } else {
                e.whyItMatters = QString(
                    "Without type annotations, mypy and pyright cannot statically verify "
                    "call sites. At runtime, CPython performs no type checking, but unannotated "
                    "code prevents static analysis from catching AttributeError/TypeError bugs "
                    "before deployment. Type hints also enable faster attribute lookup in "
                    "some JIT implementations (PyPy, Cython typed mode)."
                );
            }
            e.originalCode  = QString("def %1(%2):").arg(m.captured(1), params);
            e.suggestedCode = QString(
                "# Add a type after each parameter name with a colon,\n"
                "# and a return type after the closing parenthesis with '->':\n"
                "def %1(param_name: int, other: str) -> bool:\n"
                "    ..."
            ).arg(m.captured(1));
            e.lineNumber = i + 1;
            results.append(e);
        }
    }

    // ── 6. Global variable access inside functions ───────────────────────
    QRegularExpression globalRe(R"(^\s*global\s+(\w+))");
    for (int i = 0; i < lines.size(); ++i) {
        auto m = globalRe.match(lines[i]);
        if (!m.hasMatch()) continue;
        OptimizationEntry e;
        e.title       = "Global variable access in function";
        e.description = QString(
            "The 'global %1' keyword forces Python to look up '%1' in the module's "
            "global namespace on every access.").arg(m.captured(1));
        if (level <= 2) {
            e.whyItMatters = QString(
                "Python has to search in two places for a variable: first locally inside "
                "the function, then globally in the module. When you use 'global %1', every "
                "read of %1 does that two-step search every single time.\n\n"
                "Local variables are stored in a simple array and accessed by index \xe2\x80\x94 "
                "that's very fast. Global variables require a dictionary lookup \xe2\x80\x94 slower.\n\n"
                "The fix is to pass the value as a function parameter instead. This makes "
                "it a local variable inside the function, which is faster and also makes "
                "your function easier to test and reuse."
            ).arg(m.captured(1));
        } else {
            e.whyItMatters = QString(
                "CPython stores locals in a fast array (LOAD_FAST, O(1) array index). "
                "Globals are stored in the module's __dict__ (LOAD_GLOBAL, dict lookup). "
                "Inside a hot loop, the dict overhead per iteration compounds. Pass the "
                "value as a parameter to make it a local, or cache it: "
                "'_%1 = %1' at the top of the function."
            ).arg(m.captured(1));
        }
        e.originalCode  = lines[i].trimmed();
        e.suggestedCode = QString(
            "# Instead of 'global %1', pass it as a parameter:\n"
            "def your_function(%1):  # receives the value directly\n"
            "    # use %1 here as a local variable\n"
            "    ..."
        ).arg(m.captured(1));
        e.lineNumber = i + 1;
        results.append(e);
    }

    // ── 7. Import inside a function ──────────────────────────────────────
    QRegularExpression funcBodyImportRe(
        R"(^(\s{4,})import\s+|^(\s{4,})from\s+\S+\s+import\s+)");
    for (int i = 0; i < lines.size(); ++i) {
        if (funcBodyImportRe.match(lines[i]).hasMatch()) {
            OptimizationEntry e;
            e.title       = "Import inside a function";
            e.description = "An import statement inside a function body runs the import "
                            "machinery every time the function is called.";
            if (level <= 2) {
                e.whyItMatters =
                    "Normally Python is smart: once a module is imported, it remembers it "
                    "and reuses it next time. But when an import is inside a function, Python "
                    "still has to check every single time the function runs \xe2\x80\x94 it looks up "
                    "the module name, checks if it's already loaded, and finds it in memory.\n\n"
                    "This happens even though the module is already there. It's a small cost "
                    "each time, but if your function is called thousands of times (e.g., in "
                    "a loop), those small costs add up.\n\n"
                    "Move the import to the top of the file. Python does the check once when "
                    "the program starts, then never again.";
            } else {
                e.whyItMatters =
                    "Even though sys.modules caches loaded modules, each call still executes "
                    "the import statement: the interpreter does a dict lookup in sys.modules, "
                    "resolves the name, and stores the result. In a hot path this is measurable. "
                    "Module-level imports run once at load time. Move them there.";
            }
            e.originalCode  = lines[i].trimmed();
            e.suggestedCode = QString(
                "# Move this to the top of the file (line 1-ish):\n"
                "%1\n\n"
                "# Then remove it from inside the function."
            ).arg(lines[i].trimmed());
            e.lineNumber = i + 1;
            results.append(e);
        }
    }

    return results;
}

// ============================================================================
// C / C++ analysis
// ============================================================================
QList<OptimizationEntry> CodeOptimizer::analyzeCpp(const QStringList& lines,
                                                    bool isCpp,
                                                    int level) const
{
    QList<OptimizationEntry> results;

    // ── 1. Non-const variables never modified ────────────────────────────
    QRegularExpression varDeclRe(
        R"(^\s*(?:int|double|float|long|char|bool|size_t)\s+(\w+)\s*=)");
    for (int i = 0; i < lines.size(); ++i) {
        auto m = varDeclRe.match(lines[i]);
        if (!m.hasMatch()) continue;
        QString varName = m.captured(1);
        QRegularExpression reassignRe(
            QString(R"(\b%1\s*(?:\+|-|\*|/|%|&|\||\^)?=(?!=))")
                .arg(QRegularExpression::escape(varName)));
        bool reassigned = false;
        for (int j = i + 1; j < lines.size(); ++j) {
            if (reassignRe.match(lines[j]).hasMatch()) { reassigned = true; break; }
        }
        if (!reassigned) {
            OptimizationEntry e;
            e.title       = "Variable could be const";
            e.description = QString(
                "'%1' is set once and never changed \xe2\x80\x94 it should be marked 'const'.")
                .arg(varName);
            if (level <= 2) {
                e.whyItMatters = QString(
                    "When you mark a variable 'const', you're telling the compiler "
                    "\"this value will never change after it's set.\" That's a powerful "
                    "promise, and the compiler can take advantage of it:\n\n"
                    "\xe2\x80\xa2 It can store the value in a CPU register permanently instead of "
                    "reloading it from memory each time.\n"
                    "\xe2\x80\xa2 It can substitute the value directly into the code (constant folding).\n"
                    "\xe2\x80\xa2 It can eliminate branches that would never be taken.\n\n"
                    "It also protects you from accidentally changing the value later, which "
                    "is a common source of hard-to-find bugs."
                ).arg(varName);
            } else {
                e.whyItMatters = QString(
                    "const enables the compiler to constant-fold, promote to register, "
                    "and prove the value doesn't alias \xe2\x80\x94 opening up optimisations like "
                    "loop hoisting and CSE (common subexpression elimination). With -O2 "
                    "GCC/Clang will often optimise this anyway, but const is also a "
                    "correctness guarantee and an aid to alias analysis."
                ).arg(varName);
            }
            e.originalCode  = lines[i].trimmed();
            e.suggestedCode = "const " + lines[i].trimmed();
            e.lineNumber    = i + 1;
            results.append(e);
        }
    }

    // ── 2. malloc/calloc for single objects ──────────────────────────────
    QRegularExpression mallocRe(
        R"(\bmalloc\s*\(\s*sizeof\s*\(\s*(\w+)\s*\)\s*\))");
    for (int i = 0; i < lines.size(); ++i) {
        auto m = mallocRe.match(lines[i]);
        if (!m.hasMatch()) continue;
        QString type = m.captured(1);
        OptimizationEntry e;
        e.title       = "Heap allocation for a single object";
        e.description = QString(
            "malloc(sizeof(%1)) allocates one object on the heap when the stack "
            "would work just as well.").arg(type);
        if (level <= 2) {
            e.whyItMatters = QString(
                "Your computer has two main places to store data: the stack and the heap.\n\n"
                "The stack is like your desk \xe2\x80\x94 fast to put things on, fast to clear off, "
                "and automatically tidied when a function ends. The heap is like a storage "
                "room \xe2\x80\x94 you can keep things there longer, but you have to fetch a key "
                "(ask the OS for memory with malloc), and you have to return it yourself "
                "(free()), or it leaks.\n\n"
                "For a single %1, just declare it on the stack: '%1 obj;' \xe2\x80\x94 no malloc, "
                "no free, no risk of forgetting to clean up, and faster because the OS "
                "isn't involved at all."
            ).arg(type);
        } else {
            e.whyItMatters = QString(
                "malloc() calls into the allocator (ptmalloc/jemalloc/tcmalloc), which "
                "may acquire a lock, search free lists, and call brk/mmap. Stack allocation "
                "is a single SUB instruction on the stack pointer \xe2\x80\x94 effectively free. "
                "For a single-object %1 whose lifetime is function-scoped, stack allocation "
                "is always preferable. Also eliminates the need for free() and the risk of "
                "a memory leak."
            ).arg(type);
        }
        e.originalCode  = lines[i].trimmed();
        e.suggestedCode = QString(
            "// Instead of:\n"
            "// %1* obj = malloc(sizeof(%1));\n"
            "// ...\n"
            "// free(obj);\n\n"
            "// Use stack allocation:\n"
            "%1 obj;  // automatically freed when this scope ends\n"
            "// access via obj.field instead of obj->field"
        ).arg(type);
        e.lineNumber = i + 1;
        results.append(e);
    }

    // ── 3. Pass-by-value of large types (C++ only) ───────────────────────
    if (isCpp) {
        QRegularExpression paramRe(
            R"(^\s*\w[\w:<>]*\s+(\w+)\s*\(([^)]+)\))");
        for (int i = 0; i < lines.size(); ++i) {
            const QString& line = lines[i];
            if (!paramRe.match(line).hasMatch()) continue;
            int parenOpen  = line.indexOf('(');
            int parenClose = line.lastIndexOf(')');
            if (parenOpen < 0 || parenClose < 0) continue;
            QString params = line.mid(parenOpen + 1, parenClose - parenOpen - 1);
            static QRegularExpression primRe(
                R"(^(?:int|long|short|char|float|double|bool|void|size_t|uint|unsigned)\b)");
            for (const QString& p : params.split(',')) {
                QString pt = p.trimmed();
                if (pt.contains('&') || pt.contains('*') || pt.isEmpty()) continue;
                if (primRe.match(pt).hasMatch()) continue;
                if (pt.isEmpty() || !pt[0].isUpper()) continue;
                QString typeName = pt.split(' ')[0];
                OptimizationEntry e;
                e.title       = "Pass large type by value";
                e.description = QString(
                    "Passing '%1' by value makes a full copy of the object every time "
                    "this function is called.").arg(typeName);
                if (level <= 2) {
                    e.whyItMatters = QString(
                        "When you pass an object to a function in C++, by default the function "
                        "gets its own private copy \xe2\x80\x94 like handing someone a photocopy of a document "
                        "instead of the original. For small things like int or double, this is fine. "
                        "For larger objects like strings, vectors, or your own structs, copying "
                        "can be expensive: it allocates memory, copies every field, and then "
                        "destroys the copy when the function ends.\n\n"
                        "Use 'const %1&' instead \xe2\x80\x94 this passes a reference (like handing over "
                        "the original document to read). The function can read it but not "
                        "accidentally change it, and no copy is made."
                    ).arg(typeName);
                } else {
                    e.whyItMatters = QString(
                        "Pass-by-value invokes the copy constructor, which for non-trivial "
                        "types may allocate heap memory (e.g., std::string, std::vector). "
                        "Use 'const %1&' to pass a const reference: zero copy cost, same "
                        "read access, compiler enforces immutability. If ownership transfer "
                        "is needed, use move semantics ('%1&&' + std::move at call site)."
                    ).arg(typeName);
                }
                e.originalCode  = line.trimmed();
                QString suggested = line;
                suggested.replace(typeName, "const " + typeName + "&");
                e.suggestedCode = suggested.trimmed();
                e.lineNumber    = i + 1;
                results.append(e);
                break;
            }
        }
    }

    // ── 4. Small functions that could be inline ──────────────────────────
    QRegularExpression funcOpenRe(
        R"(^\s*\w[\w:<>*&\s]+\s+(\w+)\s*\([^)]*\)\s*\{?\s*$)");
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        if (line.contains("inline") || line.contains("//")) continue;
        auto m = funcOpenRe.match(line);
        if (!m.hasMatch()) continue;

        int braceDepth = 0, bodyLines = 0;
        for (int j = i; j < lines.size() && j < i + 20; ++j) {
            braceDepth += lines[j].count('{') - lines[j].count('}');
            if (j > i) ++bodyLines;
            if (j > i && braceDepth <= 0) break;
        }
        QString funcName = m.captured(1);
        if (funcName.isEmpty() || funcName == "main" ||
            funcName == "if" || funcName == "for" || funcName == "while") continue;

        int callCount = 0;
        QRegularExpression callRe(
            QString(R"(\b%1\s*\()").arg(QRegularExpression::escape(funcName)));
        for (const QString& l : lines)
            if (callRe.match(l).hasMatch()) ++callCount;

        if (bodyLines > 0 && bodyLines <= 4 && callCount >= 2) {
            OptimizationEntry e;
            e.title       = "Small function could be inlined";
            e.description = QString(
                "'%1' has a short body (%2 lines) and is called %3 time(s). "
                "Adding 'inline' lets the compiler skip the function call overhead.")
                .arg(funcName).arg(bodyLines).arg(callCount - 1);
            if (level <= 2) {
                e.whyItMatters = QString(
                    "Every time your program calls a function, there's a small cost: it "
                    "saves the current position, jumps to the function's code, does the work, "
                    "then jumps back. For tiny functions like getters or small helpers, "
                    "this overhead can be as expensive as the actual work.\n\n"
                    "The 'inline' keyword asks the compiler to paste the function's code "
                    "directly at each call site, eliminating those jumps entirely. The "
                    "code becomes slightly larger, but the calls become faster \xe2\x80\x94 especially "
                    "if '%1' is called inside a loop."
                ).arg(funcName);
            } else {
                e.whyItMatters = QString(
                    "Function calls incur prologue/epilogue overhead: CALL instruction, "
                    "stack frame setup, parameter passing (may spill registers), and RET. "
                    "For a %1-line body called %2 times, the overhead may dominate the "
                    "actual work. 'inline' hints the compiler to expand the body at call "
                    "sites; with -O2, GCC/Clang will often do this regardless, but the "
                    "hint is useful for header-defined functions."
                ).arg(bodyLines).arg(callCount - 1);
            }
            e.originalCode  = line.trimmed();
            e.suggestedCode = "inline " + line.trimmed();
            e.lineNumber    = i + 1;
            results.append(e);
        }
    }

    return results;
}
