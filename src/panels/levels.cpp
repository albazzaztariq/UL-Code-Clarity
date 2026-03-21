#include "panels/levels.h"
#include "core/theme.h"

using namespace Theme::Css;

LevelSelector::LevelSelector(QWidget *parent)
    : QWidget(parent), m_level(1)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(0);

    m_label = new QLabel("Assist Level", this);
    m_label->setStyleSheet(QString(
        "QLabel { font-size: 10px; font-weight: 700; color: %1;"
        " letter-spacing: 0.5px; padding-right: 6px; }"
    ).arg(FG2));
    layout->addWidget(m_label);

    // Segmented button group: L1 | L2 | L3 | L4
    struct BtnInfo { int level; QString text; };
    QVector<BtnInfo> items = {
        {1, "1 Beginner"},
        {2, "2 Intermediate"},
        {3, "3 Developer"},
        {4, "4 No Assist"}
    };

    for (int i = 0; i < items.size(); ++i) {
        auto* btn = new QPushButton(items[i].text, this);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(24);
        btn->setProperty("levelIndex", items[i].level);

        btn->setStyleSheet(QString(
            "QPushButton {"
            "  font-size: 10px; font-weight: 500; padding: 3px 12px;"
            "  background: %1; color: %2; border: 1px solid %3;"
            "  border-radius: 12px; margin: 0 2px;"
            "}"
            "QPushButton:hover {"
            "  background: %3; color: %4;"
            "}"
        ).arg(BG3, FG2, BORDER, FG));

        connect(btn, &QPushButton::clicked, this, [this, level = items[i].level]() {
            setLevel(level);
        });

        m_buttons.append(btn);
        layout->addWidget(btn);
    }

    updateButtonStyles();
}

int LevelSelector::currentLevel() const
{
    return m_level;
}

QString LevelSelector::currentLevelName() const
{
    return levelName(m_level);
}

QString LevelSelector::levelName(int level)
{
    switch (level) {
    case 1: return "Beginner";
    case 2: return "Intermediate";
    case 3: return "Developer";
    case 4: return "No Assist";
    default: return "Unknown";
    }
}

void LevelSelector::setLevel(int level)
{
    if (level < 1) level = 1;
    if (level > 4) level = 4;
    if (m_level == level) return;
    m_level = level;

    updateButtonStyles();
    applyLevel(level);
    emit levelChanged(level);
}

void LevelSelector::updateButtonStyles()
{
    if (m_isDark) {
        m_label->setStyleSheet(QString(
            "QLabel { font-size: 10px; font-weight: 700; color: %1;"
            " letter-spacing: 0.5px; padding-right: 6px; }"
        ).arg(FG2));
    } else {
        m_label->setStyleSheet(
            "QLabel { font-size: 10px; font-weight: 700; color: #555555;"
            " letter-spacing: 0.5px; padding-right: 6px; }");
    }

    for (int i = 0; i < m_buttons.size(); ++i) {
        auto* btn = m_buttons[i];
        int btnLevel = btn->property("levelIndex").toInt();
        bool active = (btnLevel == m_level);

        if (m_isDark) {
            if (active) {
                btn->setStyleSheet(QString(
                    "QPushButton {"
                    "  font-size: 10px; font-weight: 700; padding: 3px 12px;"
                    "  background: rgba(137,180,250,0.25); color: %1;"
                    "  border: 1px solid %1; border-radius: 12px; margin: 0 2px;"
                    "}"
                    "QPushButton:hover {"
                    "  background: rgba(137,180,250,0.35);"
                    "}"
                ).arg(ACCENT));
            } else {
                btn->setStyleSheet(QString(
                    "QPushButton {"
                    "  font-size: 10px; font-weight: 500; padding: 3px 12px;"
                    "  background: %1; color: %2; border: 1px solid %3;"
                    "  border-radius: 12px; margin: 0 2px;"
                    "}"
                    "QPushButton:hover {"
                    "  background: %3; color: %4;"
                    "}"
                ).arg(BG3, FG2, BORDER, FG));
            }
        } else {
            if (active) {
                btn->setStyleSheet(
                    "QPushButton {"
                    "  font-size: 10px; font-weight: 700; padding: 3px 12px;"
                    "  background: #2563eb; color: #ffffff;"
                    "  border: 1px solid #2563eb; border-radius: 12px; margin: 0 2px;"
                    "}"
                    "QPushButton:hover {"
                    "  background: #1d4ed8;"
                    "}");
            } else {
                btn->setStyleSheet(QString(
                    "QPushButton {"
                    "  font-size: 10px; font-weight: 500; padding: 3px 12px;"
                    "  background: %1; color: %2; border: 1px solid %3;"
                    "  border-radius: 12px; margin: 0 2px;"
                    "}"
                    "QPushButton:hover {"
                    "  background: %3; color: %4;"
                    "}"
                ).arg(L_BG, L_FG2, L_BORDER, L_FG));
            }
        }
    }
}

void LevelSelector::applyTheme(bool isDark)
{
    m_isDark = isDark;
    updateButtonStyles();
}

void LevelSelector::applyLevel(int level)
{
    emit showCloseButtons(DisclosureRules::showCloseButtons(level));
    emit showBuildButton(DisclosureRules::showBuildButton(level));
    emit showBasicsButton(DisclosureRules::showBasicsButton(level));
    emit showFileTree(DisclosureRules::showFileTree(level));
    emit showTerminal(DisclosureRules::showTerminal(level));
    emit showBuildTools(DisclosureRules::showBuildTools(level));
    emit showExtensions(DisclosureRules::showExtensions(level));
}
