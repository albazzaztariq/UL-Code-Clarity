#ifndef LEVELS_H
#define LEVELS_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QString>
#include <QVector>

// Level selector widget and progressive disclosure controller
// L1=Beginner, L2=Intermediate, L3=Developer, L4=No Assist
class LevelSelector : public QWidget {
    Q_OBJECT
public:
    explicit LevelSelector(QWidget *parent = nullptr);

    int currentLevel() const;
    QString currentLevelName() const;

    static QString levelName(int level);

public slots:
    void setLevel(int level);

signals:
    void levelChanged(int level);

    // UI visibility signals for progressive disclosure
    void showCloseButtons(bool show);
    void showBuildButton(bool show);
    void showBasicsButton(bool show);
    void showFileTree(bool show);
    void showTerminal(bool show);
    void showBuildTools(bool show);
    void showExtensions(bool show);

private:
    int m_level = 1;
    QLabel *m_label;
    QVector<QPushButton*> m_buttons;

    void applyLevel(int level);
    void updateButtonStyles();
};

// Progressive disclosure rules
struct DisclosureRules {
    static bool showCloseButtons(int level)  { return level >= 2; }
    static bool showBuildButton(int level)   { return level >= 2; }
    static bool showBasicsButton(int level)  { return level <= 2; }
    static bool showFileTree(int level)      { return level >= 2; }
    static bool showTerminal(int level)      { return level >= 3; }
    static bool showBuildTools(int level)    { return level >= 3; }
    static bool showExtensions(int level)    { return level >= 4; }
};

#endif // LEVELS_H
