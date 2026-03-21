#include "core/whatswrong.h"
#include <QRegularExpression>

WhatsWrongAnalyzer::WhatsWrongAnalyzer(QObject* parent)
    : QObject(parent)
{
}

QList<PossibleCause> WhatsWrongAnalyzer::analyzeError(const QString& errorMessage,
                                                        const QString& code,
                                                        int crashLine) const
{
    QList<PossibleCause> causes;
    QString msg = errorMessage.toLower();

    if (msg.contains("nameerror"))
        causes << nameError(errorMessage, crashLine);

    if (msg.contains("indexerror"))
        causes << indexError(errorMessage, code, crashLine);

    if (msg.contains("typeerror"))
        causes << typeError(errorMessage, crashLine);

    if (msg.contains("zerodivisionerror"))
        causes << zeroDivisionError(crashLine);

    if (msg.contains("attributeerror"))
        causes << attributeError(errorMessage, crashLine);

    if (msg.contains("filenotfounderror") || msg.contains("no such file"))
        causes << fileNotFoundError(errorMessage, crashLine);

    if (msg.contains("segmentation fault") || msg.contains("segfault") ||
        msg.contains("access violation"))
        causes << segfaultError(crashLine);

    if (msg.contains("valueerror"))
        causes << valueError(errorMessage, crashLine);

    if (msg.contains("keyerror"))
        causes << keyError(errorMessage, crashLine);

    if (msg.contains("recursionerror") || msg.contains("maximum recursion"))
        causes << recursionError(crashLine);

    if (msg.contains("importerror") || msg.contains("modulenotfounderror"))
        causes << importError(errorMessage, crashLine);

    if (msg.contains("indentationerror"))
        causes << indentationError(crashLine);

    if (msg.contains("syntaxerror"))
        causes << syntaxError(crashLine);

    return causes;
}

// ── Pattern handlers ──────────────────────────────────────────────────────

PossibleCause WhatsWrongAnalyzer::nameError(const QString& msg, int line) const
{
    // Extract the variable name: NameError: name 'x' is not defined
    QString varName;
    QRegularExpression re("name '([^']+)' is not defined");
    auto match = re.match(msg);
    if (match.hasMatch())
        varName = match.captured(1);

    PossibleCause c;
    c.title        = "Variable not defined";
    c.relevantLine = line;

    if (!varName.isEmpty()) {
        c.explanation  = QString("'%1' doesn't exist at this point — it was never created, "
                                  "or was created in a different scope (e.g. inside a function).").arg(varName);
        c.suggestedFix = QString("Check that '%1' is spelled correctly and created before this line.").arg(varName);
    } else {
        c.explanation  = "A variable name was used before it was created, or was misspelled.";
        c.suggestedFix = "Check spelling and that the variable is created before use.";
    }
    return c;
}

PossibleCause WhatsWrongAnalyzer::indexError(const QString& msg, const QString& /*code*/, int line) const
{
    PossibleCause c;
    c.title        = "Array index out of bounds";
    c.relevantLine = line;
    c.explanation  = "You accessed a position in a list/array that doesn't exist. "
                     "Lists start at index 0, so a list of 5 items has indices 0-4.";
    c.suggestedFix = "Check the array length before accessing. Use len(arr) and make sure your index < len(arr).";
    Q_UNUSED(msg);
    return c;
}

PossibleCause WhatsWrongAnalyzer::typeError(const QString& msg, int line) const
{
    PossibleCause c;
    c.title        = "Wrong type";
    c.relevantLine = line;

    // Check for unsupported operand types pattern
    QRegularExpression re("unsupported operand type\\(s\\) for ([^:]+): '([^']+)' and '([^']+)'");
    auto match = re.match(msg);
    if (match.hasMatch()) {
        c.explanation  = QString("Cannot use '%1' between a %2 and a %3.")
                             .arg(match.captured(1), match.captured(2), match.captured(3));
        c.suggestedFix = "Convert one side to the right type, e.g. int(x) or str(x).";
    } else {
        // Check for 'argument must be...' style
        QRegularExpression re2("'([^']+)' object is not (\\w+)");
        auto m2 = re2.match(msg);
        if (m2.hasMatch()) {
            c.explanation = QString("A '%1' object cannot be %2 here.")
                                .arg(m2.captured(1), m2.captured(2));
        } else {
            c.explanation  = "A value of the wrong type was passed or used. "
                             "E.g. a number was expected but a string was given.";
        }
        c.suggestedFix = "Check the types of all values on this line using type(x).";
    }
    return c;
}

PossibleCause WhatsWrongAnalyzer::zeroDivisionError(int line) const
{
    PossibleCause c;
    c.title        = "Division by zero";
    c.relevantLine = line;
    c.explanation  = "The program divided a number by zero, which is mathematically undefined.";
    c.suggestedFix = "Add a check before dividing: if denominator != 0: ... else: handle_zero()";
    return c;
}

PossibleCause WhatsWrongAnalyzer::attributeError(const QString& msg, int line) const
{
    PossibleCause c;
    c.title        = "Attribute does not exist";
    c.relevantLine = line;

    // 'NoneType' object has no attribute '...'
    QRegularExpression noneRe("'NoneType' object has no attribute '([^']+)'");
    auto noneMatch = noneRe.match(msg);
    if (noneMatch.hasMatch()) {
        c.explanation  = QString("The variable is None (empty/null), so calling .%1 on it fails. "
                                  "The function that was supposed to return an object returned None instead.")
                             .arg(noneMatch.captured(1));
        c.suggestedFix = "Check that the object was actually created and is not None before calling methods on it.";
    } else {
        QRegularExpression re("'([^']+)' object has no attribute '([^']+)'");
        auto m = re.match(msg);
        if (m.hasMatch()) {
            c.explanation = QString("A '%1' object doesn't have an attribute called '%2'. "
                                     "Check the object type and its available attributes.")
                                .arg(m.captured(1), m.captured(2));
        } else {
            c.explanation = "Called a method or accessed a field that doesn't exist on this object.";
        }
        c.suggestedFix = "Use dir(obj) to see what attributes are available.";
    }
    return c;
}

PossibleCause WhatsWrongAnalyzer::fileNotFoundError(const QString& msg, int line) const
{
    PossibleCause c;
    c.title        = "File not found";
    c.relevantLine = line;

    QRegularExpression re("No such file or directory: '([^']+)'");
    auto m = re.match(msg);
    if (m.hasMatch()) {
        c.explanation  = QString("The file '%1' doesn't exist at that path. "
                                  "The path may be wrong, or the file hasn't been created yet.")
                             .arg(m.captured(1));
    } else {
        c.explanation = "A file path was provided that doesn't exist on disk.";
    }
    c.suggestedFix = "Check the file path is correct. Use an absolute path or verify the working directory.";
    return c;
}

PossibleCause WhatsWrongAnalyzer::segfaultError(int line) const
{
    PossibleCause c;
    c.title        = "Memory access violation (segfault)";
    c.relevantLine = line;
    c.explanation  = "The program tried to access memory it doesn't own. "
                     "Common causes: buffer overflow, writing past the end of an array, "
                     "or following a null/dangling pointer.";
    c.suggestedFix = "Check all array accesses are within bounds. Verify pointers are not NULL before dereferencing.";
    return c;
}

PossibleCause WhatsWrongAnalyzer::valueError(const QString& msg, int line) const
{
    PossibleCause c;
    c.title        = "Invalid value";
    c.relevantLine = line;

    // int('abc') style
    QRegularExpression re("invalid literal for (\\w+)\\(\\) with base \\d+: '([^']+)'");
    auto m = re.match(msg);
    if (m.hasMatch()) {
        c.explanation  = QString("'%2' cannot be converted to %1. "
                                  "The string contains characters that are not a valid number.")
                             .arg(m.captured(1), m.captured(2));
        c.suggestedFix = "Validate the input before converting, e.g. if x.isdigit(): int(x)";
    } else {
        c.explanation  = "A value was passed that is the right type but outside the acceptable range or format.";
        c.suggestedFix = "Check what values are valid for this function and validate input before passing it.";
    }
    return c;
}

PossibleCause WhatsWrongAnalyzer::keyError(const QString& msg, int line) const
{
    PossibleCause c;
    c.title        = "Dictionary key not found";
    c.relevantLine = line;

    QRegularExpression re("KeyError: '?([^'\"\\n]+)'?");
    auto m = re.match(msg);
    if (m.hasMatch()) {
        c.explanation  = QString("The key '%1' doesn't exist in the dictionary at this point.")
                             .arg(m.captured(1).trimmed());
        c.suggestedFix = QString("Use dict.get('%1', default) instead of dict['%1'] to avoid the crash.")
                             .arg(m.captured(1).trimmed());
    } else {
        c.explanation  = "Tried to access a dictionary key that doesn't exist.";
        c.suggestedFix = "Use 'key in dict' to check before accessing, or use dict.get(key, default).";
    }
    return c;
}

PossibleCause WhatsWrongAnalyzer::recursionError(int line) const
{
    PossibleCause c;
    c.title        = "Infinite recursion";
    c.relevantLine = line;
    c.explanation  = "A function kept calling itself without stopping. "
                     "The call stack grew until Python ran out of room.";
    c.suggestedFix = "Make sure your recursive function has a base case that stops recursion. "
                     "Check that the base case is reachable.";
    return c;
}

PossibleCause WhatsWrongAnalyzer::importError(const QString& msg, int line) const
{
    PossibleCause c;
    c.title        = "Module not found";
    c.relevantLine = line;

    QRegularExpression re("No module named '([^']+)'");
    auto m = re.match(msg);
    if (m.hasMatch()) {
        c.explanation  = QString("Python can't find the module '%1'. "
                                  "It may not be installed, or the name might be misspelled.")
                             .arg(m.captured(1));
        c.suggestedFix = QString("Run: pip install %1").arg(m.captured(1));
    } else {
        c.explanation  = "A required module or package is missing.";
        c.suggestedFix = "Check the module name and install it if needed with pip install <name>.";
    }
    return c;
}

PossibleCause WhatsWrongAnalyzer::indentationError(int line) const
{
    PossibleCause c;
    c.title        = "Indentation error";
    c.relevantLine = line;
    c.explanation  = "Python is very strict about indentation (spaces/tabs at the start of lines). "
                     "A line is not indented consistently with the block it belongs to.";
    c.suggestedFix = "Use 4 spaces per indent level consistently. Don't mix tabs and spaces.";
    return c;
}

PossibleCause WhatsWrongAnalyzer::syntaxError(int line) const
{
    PossibleCause c;
    c.title        = "Syntax error";
    c.relevantLine = line;
    c.explanation  = "Python couldn't parse the code — something is written incorrectly. "
                     "Common causes: missing colon after if/for/def, mismatched parentheses, "
                     "or a typo in a keyword.";
    c.suggestedFix = "Check the line and the line before it for missing colons, brackets, or quotes.";
    return c;
}
