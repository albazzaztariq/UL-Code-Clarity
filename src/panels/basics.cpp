#include "panels/basics.h"
#include "core/jsonloader.h"
#include "core/theme.h"

#include <QKeyEvent>
#include <QScrollBar>
#include <QJsonObject>
#include <QJsonArray>

using namespace Theme::Css;

// ============================================================================
// BasicsOverlay
// ============================================================================

BasicsOverlay::BasicsOverlay(QWidget *parent)
    : QWidget(parent), m_currentPage(0)
{
    initPages();
    buildUI();
    setVisible(false);
}

void BasicsOverlay::initPages()
{
    QJsonArray pagesArr = JsonLoader::loadArray("tutorials.json", "basics_pages");
    for (const QJsonValue& pv : pagesArr) {
        QJsonObject po = pv.toObject();
        BasicsPage page;
        page.title    = po["title"].toString();
        page.subtitle = po["subtitle"].toString();
        QJsonArray cardsArr = po["cards"].toArray();
        for (const QJsonValue& cv : cardsArr) {
            QJsonObject co = cv.toObject();
            BasicsCard card;
            card.heading = co["heading"].toString();
            card.body    = co["body"].toString();
            page.cards.append(card);
        }
        m_pages.append(page);
    }
}

void BasicsOverlay::buildUI()
{
    setStyleSheet(QString("background: %1;").arg(BG));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Close button (top-right)
    m_closeButton = new QPushButton("X Close", this);
    m_closeButton->setStyleSheet(QString(
        "QPushButton { background: #f38ba8; color: #1e1e2e; font-weight: 700; "
        "padding: 6px 14px; border-radius: 6px; font-size: 12px; }"
    ));
    m_closeButton->setFixedHeight(28);
    connect(m_closeButton, &QPushButton::clicked, this, &BasicsOverlay::closeRequested);

    auto *closeRow = new QHBoxLayout;
    closeRow->addStretch();
    closeRow->addWidget(m_closeButton);
    closeRow->setContentsMargins(0, 12, 20, 0);
    mainLayout->addLayout(closeRow);

    // Scroll area for page content
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 7px; background: transparent; }"
        "QScrollBar::handle:vertical { background: #3c3c54; border-radius: 4px; }"
    );

    m_contentWidget = new QWidget;
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(0, 0, 0, 60);
    m_contentLayout->setAlignment(Qt::AlignHCenter);

    for (int i = 0; i < m_pages.size(); i++) {
        auto *pageWidget = createPageWidget(m_pages[i]);
        pageWidget->setVisible(i == 0);
        m_contentLayout->addWidget(pageWidget);
        m_pageWidgets.append(pageWidget);
    }

    m_scrollArea->setWidget(m_contentWidget);
    mainLayout->addWidget(m_scrollArea, 1);

    // Navigation bar at the bottom
    m_navBar = new QWidget(this);
    m_navBar->setFixedHeight(50);
    m_navBar->setStyleSheet(QString(
        "background: %1; border-top: 1px solid %2;"
    ).arg(BG2, BORDER));

    auto *navLayout = new QHBoxLayout(m_navBar);
    navLayout->setContentsMargins(24, 10, 24, 10);

    m_prevButton = new QPushButton("<- Back", m_navBar);
    m_prevButton->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; padding: 6px 20px; "
        "font-size: 12px; border-radius: 6px; }"
    ).arg(BG3, FG));
    connect(m_prevButton, &QPushButton::clicked, this, &BasicsOverlay::prevPage);
    navLayout->addWidget(m_prevButton);

    navLayout->addStretch();

    // Dots row
    auto *dotsWidget = new QWidget(m_navBar);
    m_dotsLayout = new QHBoxLayout(dotsWidget);
    m_dotsLayout->setContentsMargins(0, 0, 0, 0);
    m_dotsLayout->setSpacing(4);
    for (int i = 0; i < m_pages.size(); i++) {
        auto *dot = new QPushButton(m_navBar);
        dot->setFixedSize(8, 8);
        dot->setCursor(Qt::PointingHandCursor);
        int pageIndex = i;
        connect(dot, &QPushButton::clicked, this, [this, pageIndex]() {
            goToPage(pageIndex);
        });
        m_dotsLayout->addWidget(dot);
        m_dots.append(dot);
    }
    navLayout->addWidget(dotsWidget);

    navLayout->addStretch();

    m_nextButton = new QPushButton("Next ->", m_navBar);
    m_nextButton->setStyleSheet(QString(
        "QPushButton { background: %1; color: #1e1e2e; padding: 6px 20px; "
        "font-size: 12px; border-radius: 6px; }"
    ).arg(ACCENT));
    connect(m_nextButton, &QPushButton::clicked, this, &BasicsOverlay::nextPage);
    navLayout->addWidget(m_nextButton);

    mainLayout->addWidget(m_navBar);

    updateNavigation();
}

QWidget* BasicsOverlay::createPageWidget(const BasicsPage &page)
{
    auto *widget = new QWidget;
    widget->setMaximumWidth(640);

    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(24, 32, 24, 24);
    layout->setSpacing(10);

    auto *title = new QLabel(page.title, widget);
    title->setStyleSheet(QString(
        "font-size: 22px; font-weight: 700; color: %1;"
    ).arg(ACCENT));
    layout->addWidget(title);

    auto *sub = new QLabel(page.subtitle, widget);
    sub->setStyleSheet(QString("font-size: 12px; color: %1; margin-bottom: 20px;").arg(FG3));
    layout->addWidget(sub);

    for (const auto &card : page.cards)
        layout->addWidget(createCardWidget(card));

    layout->addStretch();
    return widget;
}

QWidget* BasicsOverlay::createCardWidget(const BasicsCard &card)
{
    auto *frame = new QFrame;
    frame->setStyleSheet(QString(
        "QFrame { background: %1; border-radius: 6px; border-left: 3px solid %2; }"
    ).arg(BG2, ACCENT));

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(4);

    auto *heading = new QLabel(card.heading, frame);
    heading->setStyleSheet(QString("font-size: 13px; font-weight: 700; color: %1;").arg(FG));
    layout->addWidget(heading);

    auto *body = new QLabel(card.body, frame);
    body->setStyleSheet(QString("font-size: 12px; color: %1; line-height: 1.55;").arg(FG2));
    body->setWordWrap(true);
    body->setTextFormat(Qt::RichText);
    layout->addWidget(body);

    return frame;
}

void BasicsOverlay::goToPage(int index)
{
    if (index < 0 || index >= m_pages.size()) return;

    m_currentPage = index;

    for (int i = 0; i < m_pageWidgets.size(); i++)
        m_pageWidgets[i]->setVisible(i == index);

    m_scrollArea->verticalScrollBar()->setValue(0);

    updateNavigation();
}

void BasicsOverlay::nextPage()
{
    if (m_currentPage == m_pages.size() - 1)
        emit closeRequested();
    else
        goToPage(m_currentPage + 1);
}

void BasicsOverlay::prevPage()
{
    goToPage(m_currentPage - 1);
}

int BasicsOverlay::currentPage() const  { return m_currentPage; }
int BasicsOverlay::pageCount() const    { return m_pages.size(); }

void BasicsOverlay::updateNavigation()
{
    for (int i = 0; i < m_dots.size(); i++) {
        QString color;
        if (i == m_currentPage)      color = ACCENT;
        else if (i < m_currentPage)  color = FG3;
        else                         color = BG4;
        m_dots[i]->setStyleSheet(QString(
            "QPushButton { background: %1; border-radius: 4px; border: none; }"
        ).arg(color));
    }

    m_prevButton->setVisible(m_currentPage > 0);

    if (m_currentPage == m_pages.size() - 1) {
        m_nextButton->setText("Done");
        m_nextButton->setStyleSheet(QString(
            "QPushButton { background: %1; color: #1e1e2e; padding: 6px 20px; "
            "font-size: 12px; border-radius: 6px; }"
        ).arg(GREEN));
    } else {
        m_nextButton->setText("Next ->");
        m_nextButton->setStyleSheet(QString(
            "QPushButton { background: %1; color: #1e1e2e; padding: 6px 20px; "
            "font-size: 12px; border-radius: 6px; }"
        ).arg(ACCENT));
    }
}

void BasicsOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        emit closeRequested();
    else if (event->key() == Qt::Key_Right)
        nextPage();
    else if (event->key() == Qt::Key_Left)
        prevPage();
    QWidget::keyPressEvent(event);
}

// ============================================================================
// BuildFromScratchMode — no data to externalise, logic stays
// ============================================================================

BuildFromScratchMode::BuildFromScratchMode(QObject *parent)
    : QObject(parent), m_active(false), m_phase(Design)
{
}

bool BuildFromScratchMode::isActive() const      { return m_active; }
BuildFromScratchMode::Phase BuildFromScratchMode::currentPhase() const { return m_phase; }

QString BuildFromScratchMode::phaseLabel() const
{
    switch (m_phase) {
    case Design:             return "Phase: Design";
    case LanguageSelection:  return "Phase: Language Selection";
    case Packages:           return "Phase: Packages";
    case Architecture:       return "Phase: Architecture";
    case Building:           return "Phase: Building";
    }
    return "Phase: Unknown";
}

void BuildFromScratchMode::activate()
{
    m_active = true;
    m_phase = Design;
    emit activated();
    emit showEditorPrompt(true);
}

void BuildFromScratchMode::deactivate()
{
    m_active = false;
    m_phase = Design;
    emit deactivated();
    emit showEditorPrompt(false);
}

void BuildFromScratchMode::advancePhase()
{
    if (m_phase < Building) {
        m_phase = static_cast<Phase>(static_cast<int>(m_phase) + 1);
        emit phaseChanged(m_phase);
    }
}

QString BuildFromScratchMode::generateResponse(const QString &userInput)
{
    QString response;

    switch (m_phase) {
    case Design:
        m_projectDescription = userInput;
        response = QString(
            "Great -- so you want to build <b>%1</b>.<br><br>"
            "Let me think about the best approach:<br><br>"
            "<b>Language:</b> I'd recommend <b>Python</b> for this. Here's why:<br>"
            "* Fast to prototype -- you'll see results quickly<br>"
            "* Great libraries for what you're describing<br>"
            "* Runs on both Windows and Mac without changes<br><br>"
            "If you wanted raw speed, C would work but you'd spend 3x longer on memory "
            "management. Rust would be safe but the learning curve is steep for a first project.<br><br>"
            "<b>Does Python sound good? Or would you prefer another language?</b>"
        ).arg(userInput.left(60));
        advancePhase();
        break;

    case LanguageSelection:
        response =
            "Python it is. Now let me figure out what packages we'll need:<br><br>"
            "<b>Packages:</b><br>"
            "* <code>json</code> -- built-in, for saving/loading data to files<br>"
            "* <code>os</code> -- built-in, for file path handling<br>"
            "* <code>datetime</code> -- built-in, for timestamps<br><br>"
            "No external installs needed -- everything's included with Python.<br><br>"
            "<b>Does this look right? Any features I'm missing that might need other libraries?</b>";
        advancePhase();
        break;

    case Packages:
        response =
            "Now let's design the structure. Based on what you described, here's what we need:<br><br>"
            "<b>Functions:</b><br>"
            "* <code>add_item(name, description)</code> -- creates a new entry<br>"
            "* <code>remove_item(id)</code> -- deletes by ID<br>"
            "* <code>list_items()</code> -- shows everything<br>"
            "* <code>save_to_file(path)</code> -- writes data to disk<br>"
            "* <code>load_from_file(path)</code> -- reads data from disk<br><br>"
            "<b>Data:</b> A list of dicts, each with id, name, description, created_at, done<br><br>"
            "<b>Flow:</b> main() loads the file -> shows a menu -> user picks action -> "
            "loops until quit -> saves on exit<br><br>"
            "<b>Does this design make sense? Want to add or change anything before we start coding?</b>";
        advancePhase();
        break;

    case Architecture:
        response =
            "Let's start writing code. I'll create the first function and explain each line as we go.<br><br>"
            "<b>Ready? I'll begin with the data structure and add_item().</b><br>"
            "You'll write parts of it yourself -- I'll guide you through what each line does.";
        advancePhase();
        emit showEditorPrompt(false);
        emit scaffoldCode(QString(
            "# -- Your project: %1 --\n"
            "# We'll build this together, one function at a time.\n"
            "\n"
            "# Step 1: Define the data structure\n"
            "items = []\n"
            "\n"
            "# Step 2: Your turn -- write the add_item function\n"
            "# def add_item(name, description):\n"
            "#     ... your code here ...\n"
        ).arg(m_projectDescription.left(40)));
        break;

    case Building:
        response = "Looking good! Let me check your code and give feedback on what you've written.";
        break;
    }

    return response;
}
