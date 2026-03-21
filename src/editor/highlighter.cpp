#include "editor/highlighter.h"

SyntaxHighlighter::SyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    // Keyword format — Mauve (#cba6f7)
    m_keywordFmt.setForeground(QColor(0xcb, 0xa6, 0xf7));
    m_keywordFmt.setFontWeight(QFont::Bold);

    // Type format — Yellow (#f9e2af)
    m_typeFmt.setForeground(QColor(0xf9, 0xe2, 0xaf));

    // String format — Green (#a6e3a1)
    m_stringFmt.setForeground(QColor(0xa6, 0xe3, 0xa1));

    // Comment format — Fg3 (#6c7086) italic
    m_commentFmt.setForeground(QColor(0x6c, 0x70, 0x86));
    m_commentFmt.setFontItalic(true);

    // Number format — Peach (#fab387)
    m_numberFmt.setForeground(QColor(0xfa, 0xb3, 0x87));

    // Function format — Blue/Accent (#89b4fa)
    m_functionFmt.setForeground(QColor(0x89, 0xb4, 0xfa));

    // Builtin format — Teal (#94e2d5)
    m_builtinFmt.setForeground(QColor(0x94, 0xe2, 0xd5));

    // Preprocessor format — Red (#f38ba8)
    m_preprocessorFmt.setForeground(QColor(0xf3, 0x8b, 0xa8));

    setupPythonRules();
}

void SyntaxHighlighter::setLanguage(const QString& lang)
{
    if (m_language == lang) return;
    m_language = lang;
    m_rules.clear();
    m_hasMultiLineComment = false;
    m_hasMultiLineString = false;

    if (lang == "python")     setupPythonRules();
    else if (lang == "c")     setupCRules();
    else if (lang == "cpp")   setupCppRules();
    else if (lang == "ul")    setupULRules();
    else                      setupPythonRules();

    rehighlight();
}

void SyntaxHighlighter::highlightBlock(const QString& text)
{
    // Apply single-line rules
    for (const auto& rule : m_rules) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Multi-line comment handling (C/C++)
    if (m_hasMultiLineComment) {
        setCurrentBlockState(0);

        int startIndex = 0;
        if (previousBlockState() != 1)
            startIndex = text.indexOf(m_multiLineCommentStart);

        while (startIndex >= 0) {
            auto endMatch = m_multiLineCommentEnd.match(text, startIndex + 2);
            int endIndex = endMatch.capturedStart();
            int commentLength;

            if (endIndex == -1 || !endMatch.hasMatch()) {
                setCurrentBlockState(1);
                commentLength = text.length() - startIndex;
            } else {
                commentLength = endIndex - startIndex + endMatch.capturedLength();
            }

            setFormat(startIndex, commentLength, m_commentFmt);
            startIndex = text.indexOf(m_multiLineCommentStart, startIndex + commentLength);
        }
    }

    // Multi-line string handling (Python triple quotes)
    if (m_hasMultiLineString) {
        int state = previousBlockState();
        if (state == -1) state = 0;

        // State 2 = inside triple-quoted string
        int startIndex = 0;
        if (state != 2)
            startIndex = text.indexOf(m_multiLineStringStart);

        while (startIndex >= 0) {
            int searchFrom = (state == 2) ? 0 : startIndex + 3;
            auto endMatch = m_multiLineStringEnd.match(text, searchFrom);
            int endIndex = endMatch.capturedStart();
            int strLength;

            if (endIndex == -1 || !endMatch.hasMatch()) {
                setCurrentBlockState(2);
                strLength = text.length() - startIndex;
            } else {
                strLength = endIndex - startIndex + endMatch.capturedLength();
                if (currentBlockState() == 2)
                    setCurrentBlockState(0);
            }

            setFormat(startIndex, strLength, m_stringFmt);
            state = 0;
            startIndex = text.indexOf(m_multiLineStringStart, startIndex + strLength);
        }
    }
}

// ── Python Rules ────────────────────────────────────────────────────────
void SyntaxHighlighter::setupPythonRules()
{
    m_language = "python";

    // Keywords
    QStringList keywords = {
        "and", "as", "assert", "async", "await", "break", "class", "continue",
        "def", "del", "elif", "else", "except", "finally", "for", "from",
        "global", "if", "import", "in", "is", "lambda", "nonlocal", "not",
        "or", "pass", "raise", "return", "try", "while", "with", "yield"
    };
    for (const auto& kw : keywords)
        m_rules.append({ QRegularExpression("\\b" + kw + "\\b"), m_keywordFmt });

    // Builtins
    QStringList builtins = {
        "True", "False", "None", "print", "len", "range", "int", "str",
        "float", "list", "dict", "set", "tuple", "type", "isinstance",
        "input", "open", "sum", "min", "max", "abs", "round", "sorted",
        "enumerate", "zip", "map", "filter", "super", "self"
    };
    for (const auto& bi : builtins)
        m_rules.append({ QRegularExpression("\\b" + bi + "\\b"), m_builtinFmt });

    // Function definitions
    m_rules.append({ QRegularExpression("\\bdef\\s+(\\w+)"), m_functionFmt });

    // Function calls
    m_rules.append({ QRegularExpression("\\b([a-zA-Z_]\\w*)\\s*\\("), m_functionFmt });

    // Decorators
    m_rules.append({ QRegularExpression("@\\w+"), m_builtinFmt });

    // Numbers
    m_rules.append({ QRegularExpression("\\b\\d+\\.?\\d*([eE][+-]?\\d+)?\\b"), m_numberFmt });

    // Strings (single and double quoted)
    m_rules.append({ QRegularExpression("f?\"[^\"\\\\]*(\\\\.[^\"\\\\]*)*\""), m_stringFmt });
    m_rules.append({ QRegularExpression("f?'[^'\\\\]*(\\\\.[^'\\\\]*)*'"), m_stringFmt });

    // Comments
    m_rules.append({ QRegularExpression("#[^\n]*"), m_commentFmt });

    // Triple-quoted strings (multi-line)
    m_hasMultiLineString = true;
    m_multiLineStringStart = QRegularExpression("\"\"\"");
    m_multiLineStringEnd = QRegularExpression("\"\"\"");
}

// ── C Rules ─────────────────────────────────────────────────────────────
void SyntaxHighlighter::setupCRules()
{
    m_language = "c";

    // Keywords
    QStringList keywords = {
        "auto", "break", "case", "const", "continue", "default", "do",
        "else", "enum", "extern", "for", "goto", "if", "inline",
        "register", "restrict", "return", "sizeof", "static", "struct",
        "switch", "typedef", "union", "volatile", "while"
    };
    for (const auto& kw : keywords)
        m_rules.append({ QRegularExpression("\\b" + kw + "\\b"), m_keywordFmt });

    // Types
    QStringList types = {
        "int", "char", "float", "double", "void", "long", "short",
        "unsigned", "signed", "size_t", "bool", "NULL"
    };
    for (const auto& t : types)
        m_rules.append({ QRegularExpression("\\b" + t + "\\b"), m_typeFmt });

    // Preprocessor
    m_rules.append({ QRegularExpression("^\\s*#\\w+.*$"), m_preprocessorFmt });

    // Function calls
    m_rules.append({ QRegularExpression("\\b([a-zA-Z_]\\w*)\\s*\\("), m_functionFmt });

    // Numbers
    m_rules.append({ QRegularExpression("\\b\\d+\\.?\\d*([eE][+-]?\\d+)?[fFlLuU]*\\b"), m_numberFmt });
    m_rules.append({ QRegularExpression("\\b0[xX][0-9a-fA-F]+[uUlL]*\\b"), m_numberFmt });

    // Strings
    m_rules.append({ QRegularExpression("\"[^\"\\\\]*(\\\\.[^\"\\\\]*)*\""), m_stringFmt });
    m_rules.append({ QRegularExpression("'[^'\\\\]*(\\\\.[^'\\\\]*)*'"), m_stringFmt });

    // Single-line comments
    m_rules.append({ QRegularExpression("//[^\n]*"), m_commentFmt });

    // Multi-line comments
    m_hasMultiLineComment = true;
    m_multiLineCommentStart = QRegularExpression("/\\*");
    m_multiLineCommentEnd = QRegularExpression("\\*/");
}

// ── C++ Rules ───────────────────────────────────────────────────────────
void SyntaxHighlighter::setupCppRules()
{
    setupCRules();
    m_language = "cpp";

    // Additional C++ keywords
    QStringList cppKeywords = {
        "class", "namespace", "template", "typename", "public", "private",
        "protected", "virtual", "override", "final", "new", "delete",
        "try", "catch", "throw", "nullptr", "this", "using", "constexpr",
        "noexcept", "decltype", "auto", "static_cast", "dynamic_cast",
        "reinterpret_cast", "const_cast", "operator"
    };
    for (const auto& kw : cppKeywords)
        m_rules.append({ QRegularExpression("\\b" + kw + "\\b"), m_keywordFmt });

    // C++ types
    QStringList cppTypes = {
        "string", "vector", "map", "set", "unique_ptr", "shared_ptr",
        "array", "pair", "optional", "variant", "tuple"
    };
    for (const auto& t : cppTypes)
        m_rules.append({ QRegularExpression("\\b" + t + "\\b"), m_typeFmt });
}

// ── UniLogic Rules ──────────────────────────────────────────────────────
void SyntaxHighlighter::setupULRules()
{
    m_language = "ul";

    // Keywords
    QStringList keywords = {
        "function", "end", "if", "else", "for", "while", "return",
        "returns", "each", "in", "and", "or", "not", "true", "false",
        "null", "import", "from", "module", "struct", "enum", "match",
        "case", "break", "continue", "do", "then", "begin"
    };
    for (const auto& kw : keywords)
        m_rules.append({ QRegularExpression("\\b" + kw + "\\b"), m_keywordFmt });

    // Types
    QStringList types = {
        "int", "float", "string", "bool", "list", "map", "set",
        "void", "any", "byte", "char"
    };
    for (const auto& t : types)
        m_rules.append({ QRegularExpression("\\b" + t + "\\b"), m_typeFmt });

    // Builtins
    QStringList builtins = {
        "print", "size", "change", "push", "pop", "get", "contains",
        "memtake", "memgive", "typeof"
    };
    for (const auto& bi : builtins)
        m_rules.append({ QRegularExpression("\\b" + bi + "\\b"), m_builtinFmt });

    // Function definitions
    m_rules.append({ QRegularExpression("\\bfunction\\s+(\\w+)"), m_functionFmt });

    // Function calls
    m_rules.append({ QRegularExpression("\\b([a-zA-Z_]\\w*)\\s*\\("), m_functionFmt });

    // Numbers
    m_rules.append({ QRegularExpression("\\b\\d+\\.?\\d*\\b"), m_numberFmt });

    // Strings
    m_rules.append({ QRegularExpression("\"[^\"\\\\]*(\\\\.[^\"\\\\]*)*\""), m_stringFmt });

    // Comments (UL uses // for single-line)
    m_rules.append({ QRegularExpression("//[^\n]*"), m_commentFmt });

    // End-block keywords (highlight differently)
    m_rules.append({ QRegularExpression("\\bend\\s+(function|if|for|while|struct|enum|match)\\b"), m_keywordFmt });
}
