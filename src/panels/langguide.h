#ifndef LANGGUIDE_H
#define LANGGUIDE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QPushButton>
#include <QString>
#include <QVector>
#include <QFrame>
#include <QMap>
#include <QStringList>

// One row in the language guide feature matrix
struct LangGuideRow {
    QString label;
    QString status; // "yes", "no", "partial", "info"
    QString value;
    QString helpText;
};

// A section of the language guide (e.g. "Runtime", "Memory", "Threading")
struct LangGuideSection {
    QString name;
    QVector<LangGuideRow> rows;
};

// Full language guide data for one language
struct LangGuideData {
    QString title;    // e.g. "Python -- Language Reference"
    QString subtitle; // e.g. "Interpreted, garbage-collected..."
    QVector<LangGuideSection> sections;
};

// The Language Guide overlay (feature matrix with Yes/No/Partial)
class LangGuideOverlay : public QWidget {
    Q_OBJECT
public:
    explicit LangGuideOverlay(QWidget *parent = nullptr);

    // Show guide for a specific language
    void showForLanguage(const QString &lang);

    // Static data getters (ported from prototype LANG_TUTORIALS)
    static LangGuideData pythonGuide();
    static LangGuideData cGuide();
    static LangGuideData ulGuide();

signals:
    void closeRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QLabel *m_titleLabel;
    QLabel *m_subtitleLabel;
    QScrollArea *m_scrollArea;
    QWidget *m_bodyWidget;
    QVBoxLayout *m_bodyLayout;
    QPushButton *m_closeButton;

    void buildGuide(const LangGuideData &data);
    QWidget* createRow(const LangGuideRow &row);
    QWidget* createSectionHeader(const QString &name);

    // Tooltip helper
    void showHelpTooltip(const QString &title, const QString &body, QWidget *anchor);
};

// Explain button handler -- generates inline explanation of selected/all code
class ExplainHandler : public QObject {
    Q_OBJECT
public:
    explicit ExplainHandler(QObject *parent = nullptr);

    // Get explained version of code for a given file (static prebuilt map)
    QString getExplainedCode(const QString &filename) const;

    // Check if a prebuilt explanation exists for a file
    bool hasExplanation(const QString &filename) const;

    // Generate inline-commented version of any code via local line scanning.
    // lang: "python", "c", "cpp", "rust", "ul", "js", "text"
    // level: 1=Beginner (very verbose), 2=Intermediate, 3=Developer, 4=No Assist (signatures only)
    static QString generateExplainedCode(const QString &code,
                                         const QString &lang,
                                         int level);

private:
    QMap<QString, QString> m_explanations;
    void initExplanations();
};

#endif // LANGGUIDE_H
