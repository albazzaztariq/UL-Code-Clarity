#include "core/favoritesbar.h"
#include "core/theme.h"

#include <QSettings>
#include <QMenu>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QContextMenuEvent>
#include <QScrollArea>
#include <QFrame>

// ── Constructor ──────────────────────────────────────────────────────────────
FavoritesBar::FavoritesBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(32);
    setObjectName("FavoritesBar");

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(6, 2, 6, 2);
    m_layout->setSpacing(4);
    m_layout->addStretch();

    // Right-click on the bar background → show Add Tool
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this,
        [this](const QPoint& pos) {
            QMenu menu(this);
            auto* addAct = menu.addAction("Add Tool...");
            connect(addAct, &QAction::triggered, this, &FavoritesBar::showAddToolDialog);
            menu.exec(mapToGlobal(pos));
        });

    reload();
    applyTheme(true);
}

// ── Register a tool ──────────────────────────────────────────────────────────
void FavoritesBar::registerTool(const QString& id, const QString& label,
                                 std::function<void()> handler)
{
    m_registry[id] = { id, label, std::move(handler) };
}

// ── Reload from QSettings ────────────────────────────────────────────────────
void FavoritesBar::reload()
{
    QSettings s("CodeClarity", "CodeClarity");
    m_order = s.value("favorites/tools").toStringList();
    rebuildButtons();
}

// ── Save current order to QSettings ─────────────────────────────────────────
void FavoritesBar::saveToSettings()
{
    QSettings s("CodeClarity", "CodeClarity");
    s.setValue("favorites/tools", m_order);
}

// ── Rebuild button row from m_order ──────────────────────────────────────────
void FavoritesBar::rebuildButtons()
{
    // Remove all existing buttons from layout
    for (auto* btn : m_buttons)
        btn->deleteLater();
    m_buttons.clear();

    // Remove all items from layout (keep the trailing stretch)
    while (m_layout->count() > 0)
        m_layout->takeAt(0);

    // Add a thin separator label
    auto addSep = [&]() {
        auto* sep = new QFrame(this);
        sep->setFrameShape(QFrame::VLine);
        sep->setFixedWidth(1);
        sep->setStyleSheet(QString("QFrame { background: %1; }").arg(Theme::Colors::border()));
        m_layout->addWidget(sep);
    };

    for (int i = 0; i < m_order.size(); ++i) {
        const QString& id = m_order[i];
        if (!m_registry.contains(id)) continue;
        const ToolEntry& entry = m_registry[id];

        if (i > 0) addSep();

        auto* btn = new QPushButton(entry.label, this);
        btn->setFixedHeight(24);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFlat(false);
        btn->setContextMenuPolicy(Qt::CustomContextMenu);

        QString btnStyle = m_isDark
            ? QString("QPushButton { background: #313244; color: %1; border: 1px solid %2;"
              " border-radius: 3px; padding: 0 10px; font-size: 11px; }"
              "QPushButton:hover { background: %2; }"
              "QPushButton:pressed { background: #585b70; }")
              .arg(Theme::Colors::fg(), Theme::Colors::border())
            : "QPushButton { background: #e0e0e8; color: #313244; border: 1px solid #b0b0c0;"
              " border-radius: 3px; padding: 0 10px; font-size: 11px; }"
              "QPushButton:hover { background: #c8c8d8; }"
              "QPushButton:pressed { background: #a8a8c0; }";
        btn->setStyleSheet(btnStyle);

        connect(btn, &QPushButton::clicked, this, [entry]() {
            if (entry.handler) entry.handler();
        });

        connect(btn, &QPushButton::customContextMenuRequested, this,
            [this, id](const QPoint& pos) {
                showButtonContextMenu(id, mapToGlobal(pos));
            });

        m_buttons[id] = btn;
        m_layout->addWidget(btn);
    }

    m_layout->addStretch();
}

// ── Context menu on a button ──────────────────────────────────────────────────
void FavoritesBar::showButtonContextMenu(const QString& id, const QPoint& globalPos)
{
    QMenu menu(this);

    int idx = m_order.indexOf(id);

    auto* moveLeft = menu.addAction("Move Left");
    moveLeft->setEnabled(idx > 0);
    connect(moveLeft, &QAction::triggered, this, [this, id]() { moveButton(id, -1); });

    auto* moveRight = menu.addAction("Move Right");
    moveRight->setEnabled(idx < m_order.size() - 1);
    connect(moveRight, &QAction::triggered, this, [this, id]() { moveButton(id, +1); });

    menu.addSeparator();

    auto* remove = menu.addAction("Remove");
    connect(remove, &QAction::triggered, this, [this, id]() { removeButton(id); });

    menu.exec(globalPos);
}

// ── Show "Add Tool" picker dialog ─────────────────────────────────────────────
void FavoritesBar::showAddToolDialog()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Add Tool to Favorites");
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setMinimumSize(300, 360);

    QString dlgStyle = m_isDark
        ? QString("QDialog { background: %1; color: %2; }"
          "QListWidget { background: %3; color: %2; border: 1px solid %4;"
          " border-radius: 4px; font-size: 12px; }"
          "QListWidget::item:hover { background: #313244; }"
          "QListWidget::item:selected { background: %4; color: %2; }"
          "QLabel { color: %5; font-size: 11px; }")
          .arg(Theme::Colors::bg(), Theme::Colors::fg(),
               Theme::Colors::bg2(), Theme::Colors::border(), Theme::Colors::fg2())
        : "QDialog { background: #f0f0f8; color: #313244; }"
          "QListWidget { background: #ffffff; color: #313244; border: 1px solid #b0b0c0;"
          " border-radius: 4px; font-size: 12px; }"
          "QListWidget::item:hover { background: #e8e8f0; }"
          "QListWidget::item:selected { background: #c0c0d8; color: #313244; }"
          "QLabel { color: #606080; font-size: 11px; }";
    dlg->setStyleSheet(dlgStyle);

    auto* lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(14, 12, 14, 12);
    lay->setSpacing(8);

    auto* lbl = new QLabel("Select a tool to add to the favorites bar:", dlg);
    lay->addWidget(lbl);

    auto* list = new QListWidget(dlg);
    for (auto it = m_registry.begin(); it != m_registry.end(); ++it) {
        if (m_order.contains(it.key())) continue;  // already in bar
        auto* item = new QListWidgetItem(it.value().label, list);
        item->setData(Qt::UserRole, it.key());
    }
    list->sortItems();
    lay->addWidget(list, 1);

    auto* btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    btnBox->setStyleSheet(QString(
        "QPushButton { background: #313244; color: %1; border: 1px solid %2;"
        " border-radius: 4px; padding: 4px 14px; font-size: 12px; }"
        "QPushButton:hover { background: %2; }")
        .arg(Theme::Colors::fg(), Theme::Colors::border()));
    lay->addWidget(btnBox);

    connect(btnBox, &QDialogButtonBox::accepted, dlg, [this, dlg, list]() {
        auto* item = list->currentItem();
        if (!item) return;
        QString id = item->data(Qt::UserRole).toString();
        if (!id.isEmpty() && !m_order.contains(id)) {
            m_order.append(id);
            saveToSettings();
            rebuildButtons();
        }
        dlg->accept();
    });
    connect(btnBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, dlg, [this, dlg, list](QListWidgetItem* item) {
        Q_UNUSED(list);
        QString id = item->data(Qt::UserRole).toString();
        if (!id.isEmpty() && !m_order.contains(id)) {
            m_order.append(id);
            saveToSettings();
            rebuildButtons();
        }
        dlg->accept();
    });

    dlg->exec();
}

// ── Remove a button from the bar ─────────────────────────────────────────────
void FavoritesBar::removeButton(const QString& id)
{
    m_order.removeAll(id);
    saveToSettings();
    rebuildButtons();
}

// ── Move a button left or right ───────────────────────────────────────────────
void FavoritesBar::moveButton(const QString& id, int delta)
{
    int idx = m_order.indexOf(id);
    if (idx < 0) return;
    int newIdx = idx + delta;
    if (newIdx < 0 || newIdx >= m_order.size()) return;
    m_order.swapItemsAt(idx, newIdx);
    saveToSettings();
    rebuildButtons();
}

// ── Apply theme ───────────────────────────────────────────────────────────────
void FavoritesBar::applyTheme(bool isDark)
{
    m_isDark = isDark;
    if (isDark) {
        setStyleSheet(
            "FavoritesBar { background: #181825; border-bottom: 1px solid #313244; }");  // bg5
    } else {
        setStyleSheet(
            "FavoritesBar { background: #e8e8f0; border-bottom: 1px solid #c0c0d0; }");  // light theme literals
    }
    rebuildButtons();
}
