#pragma once

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QStringList>
#include <QMap>
#include <functional>

// ============================================================================
// FavoritesBar — customizable toolbar strip below the news ticker
//
// Users right-click the bar to add tools from a picker dialog.
// Each tool appears as a QPushButton. Right-click a button to remove it
// or assign a custom shortcut. Buttons can be reordered with left/right arrows.
// State is persisted in QSettings under key "favorites/tools".
// ============================================================================

class FavoritesBar : public QWidget {
    Q_OBJECT

public:
    explicit FavoritesBar(QWidget* parent = nullptr);

    // Register a tool that can be added to the favorites bar.
    // id       — unique settings key (e.g. "memcheck")
    // label    — display name on the button
    // handler  — called when the button is clicked
    void registerTool(const QString& id, const QString& label,
                      std::function<void()> handler);

    // Reload bar from QSettings (call after settings change)
    void reload();

    void applyTheme(bool isDark);

private:
    struct ToolEntry {
        QString            id;
        QString            label;
        std::function<void()> handler;
    };

    void rebuildButtons();
    void saveToSettings();
    void showAddToolDialog();
    void removeButton(const QString& id);
    void moveButton(const QString& id, int delta);

    // context menu on a tool button
    void showButtonContextMenu(const QString& id, const QPoint& globalPos);

    QHBoxLayout*             m_layout  = nullptr;
    QStringList              m_order;            // ordered list of tool IDs currently in bar
    QMap<QString, ToolEntry> m_registry;         // all registered tools
    QMap<QString, QPushButton*> m_buttons;       // id → button in bar

    bool m_isDark = true;
};
