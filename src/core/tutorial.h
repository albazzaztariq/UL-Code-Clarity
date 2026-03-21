#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QVector>
#include <QScrollArea>

// ============================================================================
// TutorialDialog — paginated learning module
//
// Each page has a title and rich HTML body. Navigation dots show position.
// Back / Next move between pages; Close dismisses the dialog.
//
// Usage:
//   auto* dlg = TutorialDialog::buildTarget(parentWidget);
//   dlg->exec();
//   dlg->deleteLater();
// ============================================================================
class TutorialDialog : public QDialog {
    Q_OBJECT

public:
    struct Page {
        QString title;
        QString html;   // rich-text body (may include <b>, <i>, <br>, etc.)
    };

    explicit TutorialDialog(const QString& tutorialTitle,
                            const QVector<Page>& pages,
                            QWidget* parent = nullptr);

    // Factory methods — one per tutorial topic
    static TutorialDialog* buildTarget(QWidget* parent = nullptr);
    static TutorialDialog* memory(QWidget* parent = nullptr);
    static TutorialDialog* security(QWidget* parent = nullptr);
    static TutorialDialog* codeHealth(QWidget* parent = nullptr);

private slots:
    void goNext();
    void goBack();

private:
    void showPage(int index);
    void updateDots();

    QLabel*      m_titleLabel   = nullptr;
    QLabel*      m_bodyLabel    = nullptr;
    QScrollArea* m_scrollArea   = nullptr;
    QPushButton* m_backBtn      = nullptr;
    QPushButton* m_nextBtn      = nullptr;
    QHBoxLayout* m_dotsLayout   = nullptr;

    QVector<Page>    m_pages;
    QVector<QLabel*> m_dots;
    int              m_current  = 0;
};
