#ifndef EXPANDER_H
#define EXPANDER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QFrame>
#include <QStringList>

// Statement Expander overlay -- breaks compressed/nested statements into
// one operation per line for readability
class ExpanderOverlay : public QWidget {
    Q_OBJECT
public:
    explicit ExpanderOverlay(QWidget *parent = nullptr);

    // Show the overlay with a before/after demo
    void showWithDemo(const QString &original, const QString &expanded);

    // Show the default demo from the prototype
    void showDefaultDemo();

signals:
    void expandFileRequested();   // User clicked "Expand Current File"
    void closeRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QLabel *m_titleLabel;
    QLabel *m_descLabel;
    QFrame *m_demoFrame;
    QLabel *m_originalLabel;
    QLabel *m_arrowLabel;
    QLabel *m_expandedLabel;
    QPushButton *m_expandButton;
    QPushButton *m_closeButton;
};

// "Walk Me Through This Code" button handler
// Generates a structural + execution flow explanation of the current file
// and inserts it into the AI Chat panel
class WalkThroughHandler : public QObject {
    Q_OBJECT
public:
    explicit WalkThroughHandler(QObject *parent = nullptr);

    // Generate a walkthrough explanation for a given file.
    // level: 1=Beginner, 2=Intermediate, 3=Developer, 4=No Assist
    QString generateWalkthrough(const QString &filename, const QString &code, int level = 1) const;

signals:
    // Emitted with the explanation text to be inserted into AI Chat
    void walkthroughGenerated(const QString &explanation);
};

#endif // EXPANDER_H
