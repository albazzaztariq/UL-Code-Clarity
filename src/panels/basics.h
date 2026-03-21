#ifndef BASICS_H
#define BASICS_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QVector>
#include <QFrame>

// One card within a basics page
struct BasicsCard {
    QString heading;
    QString body; // may contain HTML (<code>, <b>, <em>)
};

// One page of the Teach Me the Basics tutorial
struct BasicsPage {
    QString title;
    QString subtitle;
    QVector<BasicsCard> cards;
};

// Fullscreen "Teach Me the Basics" overlay (12 pages)
class BasicsOverlay : public QWidget {
    Q_OBJECT
public:
    explicit BasicsOverlay(QWidget *parent = nullptr);

    // Navigate
    void goToPage(int index);
    void nextPage();
    void prevPage();
    int currentPage() const;
    int pageCount() const;

signals:
    void closeRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    int m_currentPage = 0;
    QVector<BasicsPage> m_pages;
    QVector<QWidget*> m_pageWidgets;
    QVector<QPushButton*> m_dots;

    QScrollArea *m_scrollArea;
    QWidget *m_contentWidget;
    QVBoxLayout *m_contentLayout;
    QHBoxLayout *m_dotsLayout;
    QPushButton *m_prevButton;
    QPushButton *m_nextButton;
    QPushButton *m_closeButton;
    QWidget *m_navBar;

    void initPages(); // Populates m_pages from prototype data
    void buildUI();
    void updateNavigation();
    QWidget* createPageWidget(const BasicsPage &page);
    QWidget* createCardWidget(const BasicsCard &card);
};

// Build from Scratch guided mode
class BuildFromScratchMode : public QObject {
    Q_OBJECT
public:
    explicit BuildFromScratchMode(QObject *parent = nullptr);

    enum Phase { Design, LanguageSelection, Packages, Architecture, Building };

    bool isActive() const;
    Phase currentPhase() const;
    QString phaseLabel() const;

    // Generate AI response for the current phase given user input
    QString generateResponse(const QString &userInput);

    // Advance to next phase
    void advancePhase();

public slots:
    void activate();
    void deactivate();

signals:
    void activated();
    void deactivated();
    void phaseChanged(Phase phase);
    void showEditorPrompt(bool show);
    void scaffoldCode(const QString &code);

private:
    bool m_active = false;
    Phase m_phase = Design;
    QString m_projectDescription;
};

#endif // BASICS_H
