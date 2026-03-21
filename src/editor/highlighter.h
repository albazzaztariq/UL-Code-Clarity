#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>

class SyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit SyntaxHighlighter(QTextDocument* parent = nullptr);

    void setLanguage(const QString& lang);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    void setupPythonRules();
    void setupCRules();
    void setupCppRules();
    void setupULRules();

    QVector<Rule> m_rules;
    QString m_language;

    // Formats
    QTextCharFormat m_keywordFmt;
    QTextCharFormat m_typeFmt;
    QTextCharFormat m_stringFmt;
    QTextCharFormat m_commentFmt;
    QTextCharFormat m_numberFmt;
    QTextCharFormat m_functionFmt;
    QTextCharFormat m_builtinFmt;
    QTextCharFormat m_preprocessorFmt;

    // Multi-line state
    QRegularExpression m_multiLineCommentStart;
    QRegularExpression m_multiLineCommentEnd;
    QRegularExpression m_multiLineStringStart;
    QRegularExpression m_multiLineStringEnd;
    bool m_hasMultiLineComment = false;
    bool m_hasMultiLineString = false;
};
