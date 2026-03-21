#ifndef CLARITY_H
#define CLARITY_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QString>
#include <QVector>
#include <QFrame>
#include <QPushButton>
#include <QSettings>

// A single Clarity changelog entry with expandable detail
class ClarityEntry : public QFrame {
    Q_OBJECT
public:
    explicit ClarityEntry(const QString &title, const QString &time,
                          const QString &detailHtml, QWidget *parent = nullptr);

    void setExpanded(bool expanded);
    bool isExpanded() const;

signals:
    void clicked();
    void tryItYourselfClicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    QLabel *m_titleLabel;
    QLabel *m_timeLabel;
    QWidget *m_detailWidget;
    bool m_expanded = false;

    void buildDetailContent(const QString &detailHtml);
};

// Data for one changelog entry at a given level
struct ClarityEntryData {
    QString title;
    QString time;
    QString detailHtml; // structured HTML: What Changed, Why, Concepts, etc.
};

// The full Clarity changelog panel (right-side dock widget)
class ClarityPanel : public QWidget {
    Q_OBJECT
public:
    explicit ClarityPanel(QWidget *parent = nullptr);

    // Set the verbosity level (1=Beginner, 2=Intermediate, 3=Developer, 4=No Assist)
    void setLevel(int level);
    int level() const;

    // Add a new entry to the changelog
    void addEntry(const ClarityEntryData &data);

    // Clear all entries
    void clearEntries();

    // Called when user opens a real file — clears example entries
    void markExamplesSeen();

    // Apply light or dark theme
    void applyTheme(bool isDark);

signals:
    void entryClicked(int index);
    void tryItYourself(int index);
    void closeRequested();

private:
    int m_level = 1;
    bool m_isDark = true;
    QVBoxLayout *m_entriesLayout;
    QScrollArea *m_scrollArea;
    QLabel *m_headerLabel;
    QWidget *m_columnHeader;
    QPushButton *m_closeButton;
    QVector<ClarityEntry*> m_entries;

    // Prebuilt sample entries per level (ported from prototype)
    static QVector<ClarityEntryData> sampleEntries(int level);
    void rebuildEntries();
};

#endif // CLARITY_H
