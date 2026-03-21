#pragma once

#include <QColor>
#include <QString>

// Catppuccin Mocha color scheme — matches the prototype exactly
namespace Theme {

// Backgrounds
inline const QColor Bg       {0x1e, 0x1e, 0x2e};  // #1e1e2e  main bg
inline const QColor Bg2      {0x2a, 0x2a, 0x3c};  // #2a2a3c  panels, sidebars
inline const QColor Bg3      {0x33, 0x33, 0x48};  // #333348  input fields, dropdowns
inline const QColor Bg4      {0x3c, 0x3c, 0x54};  // #3c3c54  hover states
inline const QColor Bg5      {0x18, 0x18, 0x25};  // #181825  results pane

// Foregrounds
inline const QColor Fg       {0xcd, 0xd6, 0xf4};  // #cdd6f4  main text
inline const QColor Fg2      {0xa6, 0xad, 0xc8};  // #a6adc8  secondary text
inline const QColor Fg3      {0x6c, 0x70, 0x86};  // #6c7086  muted/labels

// Accents
inline const QColor Accent   {0x89, 0xb4, 0xfa};  // #89b4fa  blue
inline const QColor Accent2  {0x74, 0xc7, 0xec};  // #74c7ec  sapphire
inline const QColor Green    {0xa6, 0xe3, 0xa1};  // #a6e3a1
inline const QColor Red      {0xf3, 0x8b, 0xa8};  // #f38ba8
inline const QColor Yellow   {0xf9, 0xe2, 0xaf};  // #f9e2af
inline const QColor Mauve    {0xcb, 0xa6, 0xf7};  // #cba6f7
inline const QColor Peach    {0xfa, 0xb3, 0x87};  // #fab387
inline const QColor Teal     {0x94, 0xe2, 0xd5};  // #94e2d5

// Borders
inline const QColor Border   {0x45, 0x47, 0x5a};  // #45475a

// Fonts
inline const QString SansFont = QStringLiteral("Segoe UI");
inline const QString MonoFont = QStringLiteral("Cascadia Code");

// Stylesheet helpers
inline QString colorStr(const QColor& c) {
    return c.name(QColor::HexRgb);
}

// Global application stylesheet
inline QString appStyleSheet() {
    return QStringLiteral(
        // Base
        "QMainWindow, QWidget { background: #1e1e2e; color: #cdd6f4;"
        " font-family: 'Segoe UI'; font-size: 12px; }"

        // Menu bar — subtle bottom border, 8px horizontal padding per item
        "QMenuBar { background: #2a2a3c; color: #a6adc8;"
        " border-bottom: 1px solid #313244; font-size: 12px; padding: 1px 0; }"
        "QMenuBar::item { padding: 4px 8px; border-radius: 4px; }"
        "QMenuBar::item:selected { background: #313244; color: #cdd6f4; }"

        // Dropdown menus
        "QMenu { background: #2a2a3c; color: #a6adc8;"
        " border: 1px solid #45475a; padding: 4px 0; border-radius: 6px; }"
        "QMenu::item { padding: 5px 16px 5px 12px; }"
        "QMenu::item:selected { background: #3c3c54; color: #cdd6f4; border-radius: 3px; }"
        "QMenu::separator { height: 1px; background: #313244; margin: 3px 8px; }"

        // Status bar — slightly darker than main bg
        "QStatusBar { background: #181825; color: #a6adc8;"
        " border-top: 1px solid #313244; font-size: 11px; padding: 0 4px; }"
        "QStatusBar::item { border: none; }"

        // Splitter handles — subtle but visible
        "QSplitter::handle { background: #313244; }"
        "QSplitter::handle:horizontal { width: 1px; }"
        "QSplitter::handle:vertical { height: 1px; }"
        "QSplitter::handle:hover { background: #45475a; }"

        // Scrollbars — thin (8px), rounded thumb, transparent track
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #45475a; border-radius: 4px;"
        " min-height: 24px; margin: 2px; }"
        "QScrollBar::handle:vertical:hover { background: #585b70; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
        "QScrollBar:horizontal { background: transparent; height: 8px; margin: 0; }"
        "QScrollBar::handle:horizontal { background: #45475a; border-radius: 4px;"
        " min-width: 24px; margin: 2px; }"
        "QScrollBar::handle:horizontal:hover { background: #585b70; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }"

        // Tooltip
        "QToolTip { background: #2a2a3c; color: #cdd6f4; border: 1px solid #45475a;"
        " padding: 4px 8px; font-size: 11px; border-radius: 4px; }"

        // Push buttons — consistent border-radius 6px, padding 6px 16px
        "QPushButton { border: none; border-radius: 6px; padding: 6px 16px; font-size: 12px; }"
        "QPushButton:hover { background: #3c3c54; }"
        "QPushButton:pressed { background: #45475a; }"
        "QPushButton:disabled { color: #45475a; }"

        // Combo boxes
        "QComboBox { background: #2a2a3c; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; padding: 4px 8px; font-size: 11px; }"
        "QComboBox:focus { border-color: #89b4fa; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox::down-arrow { width: 8px; height: 8px; }"
        "QComboBox QAbstractItemView { background: #2a2a3c; color: #cdd6f4;"
        " border: none; outline: none; selection-background-color: #3c3c54; }"

        // Line edits — focus ring
        "QLineEdit { background: #2a2a3c; color: #cdd6f4; border: 1px solid #45475a;"
        " border-radius: 4px; padding: 5px 10px; font-size: 12px; }"
        "QLineEdit:focus { border-color: #89b4fa; }"

        // Tab bar — VS Code-style: active has colored bottom border
        "QTabBar::tab { background: #2a2a3c; color: #a6adc8; padding: 7px 16px;"
        " border-right: 1px solid #313244; font-size: 11px; border-bottom: 2px solid transparent; }"
        "QTabBar::tab:selected { background: #1e1e2e; color: #cdd6f4;"
        " border-bottom: 2px solid #89b4fa; }"
        "QTabBar::tab:hover:!selected { background: #313244; color: #cdd6f4; }"
        "QTabBar::close-button { subcontrol-position: right; }"

        // Tree view — row height 24px
        "QTreeView { background: #2a2a3c; color: #a6adc8; border: none; font-size: 12px; }"
        "QTreeView::item { padding: 4px 6px; min-height: 24px; }"
        "QTreeView::item:hover { background: #313244; }"
        "QTreeView::item:selected { background: transparent; color: #cdd6f4;"
        " border-left: 2px solid #89b4fa; }"
        "QTreeView::branch { background: #2a2a3c; }"

        // Header
        "QHeaderView::section { background: #2a2a3c; color: #a6adc8;"
        " border: none; padding: 4px 8px; font-size: 10px; }"

        // Text edit
        "QTextEdit { background: #1e1e2e; color: #cdd6f4; border: none; font-size: 12px; }"

        // Dialogs
        "QDialog { background: #1e1e2e; }"
        "QDialog QLabel { color: #cdd6f4; }"

    );
}

// Light theme stylesheet — complete coverage of every widget
inline QString lightStyleSheet() {
    return QStringLiteral(
        // Base
        "QMainWindow, QWidget { background: #ffffff; color: #333333;"
        " font-family: 'Segoe UI'; font-size: 12px; }"

        // Menu bar
        "QMenuBar { background: #f0f0f0; color: #333333;"
        " border-bottom: 1px solid #e0e0e0; font-size: 12px; padding: 1px 0; }"
        "QMenuBar::item { padding: 4px 8px; border-radius: 4px; }"
        "QMenuBar::item:selected { background: #e0e0e0; color: #1e1e2e; }"

        // Dropdown menus
        "QMenu { background: #ffffff; color: #1e1e2e;"
        " border: 1px solid #d0d0d0; padding: 4px 0; border-radius: 6px; }"
        "QMenu::item { padding: 5px 16px 5px 12px; }"
        "QMenu::item:selected { background: #e0e8ff; color: #1e1e2e; border-radius: 3px; }"
        "QMenu::separator { height: 1px; background: #d0d0d0; margin: 3px 8px; }"

        // Status bar
        "QStatusBar { background: #f0f0f0; color: #555555;"
        " border-top: 1px solid #e0e0e0; font-size: 11px; padding: 0 4px; }"
        "QStatusBar::item { border: none; }"

        // Splitter handles
        "QSplitter::handle { background: #e0e0e0; }"
        "QSplitter::handle:horizontal { width: 1px; }"
        "QSplitter::handle:vertical { height: 1px; }"
        "QSplitter::handle:hover { background: #c0c0c0; }"

        // Scrollbars — thin, cccccc thumb on f0f0f0 track
        "QScrollBar:vertical { background: #f0f0f0; width: 8px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #cccccc; border-radius: 4px;"
        " min-height: 24px; margin: 2px; }"
        "QScrollBar::handle:vertical:hover { background: #aaaaaa; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: #f0f0f0; }"
        "QScrollBar:horizontal { background: #f0f0f0; height: 8px; margin: 0; }"
        "QScrollBar::handle:horizontal { background: #cccccc; border-radius: 4px;"
        " min-width: 24px; margin: 2px; }"
        "QScrollBar::handle:horizontal:hover { background: #aaaaaa; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: #f0f0f0; }"

        // Tooltip
        "QToolTip { background: #ffffff; color: #1e1e2e; border: 1px solid #d0d0d0;"
        " padding: 4px 8px; font-size: 11px; border-radius: 4px; }"

        // Push buttons — clean flat, very subtle bg
        "QPushButton { background: #f0f0f0; color: #333333; border: none;"
        " border-radius: 6px; padding: 6px 16px; font-size: 12px; }"
        "QPushButton:hover { background: #e4e4e4; }"
        "QPushButton:pressed { background: #d8d8d8; }"
        "QPushButton:disabled { color: #aaaaaa; background: #f0f0f0; }"

        // Combo boxes
        "QComboBox { background: #ffffff; color: #333333; border: 1px solid #e0e0e0;"
        " border-radius: 4px; padding: 4px 8px; font-size: 11px; }"
        "QComboBox:focus { border-color: #2563eb; }"
        "QComboBox::drop-down { border-left: 1px solid #d0d0d0; width: 20px;"
        " subcontrol-position: right; subcontrol-origin: padding; background: #ffffff; }"
        "QComboBox::down-arrow { border-top: 5px solid #333333;"
        " border-left: 4px solid transparent; border-right: 4px solid transparent;"
        " width: 0; height: 0; }"
        "QComboBox QAbstractItemView { background: #ffffff; color: #333333;"
        " border: none; outline: none; selection-background-color: #e0e8ff; }"

        // Line edits
        "QLineEdit { background: #ffffff; color: #333333; border: 1px solid #e0e0e0;"
        " border-radius: 4px; padding: 5px 10px; font-size: 12px; }"
        "QLineEdit:focus { border-color: #2563eb; }"

        // Tab bar — inactive: #f0f0f0 bg, active: white with blue bottom border
        "QTabBar::tab { background: #f0f0f0; color: #555555; border: none;"
        " border-bottom: 2px solid transparent; padding: 6px 14px; font-size: 11px; }"
        "QTabBar::tab:selected { background: #ffffff; color: #1e1e2e;"
        " border-bottom: 2px solid #2563eb; }"
        "QTabBar::tab:hover:!selected { background: #e8e8e8; color: #333333; }"
        // Close button: grey X, turns red on hover
        "QTabBar::close-button { subcontrol-position: right; }"
        "QTabBar::close-button:hover { background: transparent; }"

        // Tree view
        "QTreeView { background: #fafafa; color: #333333; border: none; font-size: 12px; }"
        "QTreeView::item { padding: 4px 6px; min-height: 24px; }"
        "QTreeView::item:hover { background: #f0f0f0; }"
        "QTreeView::item:selected { background: transparent; color: #2563eb;"
        " border-left: 2px solid #2563eb; }"
        "QTreeView::branch { background: #fafafa; }"

        // Header — flat, no shadow, subtle border
        "QHeaderView::section { background: #fafafa; color: #333333;"
        " border: none; border-bottom: 1px solid #e0e0e0; padding: 4px 8px; font-size: 10px; }"

        // Text edit — white editor bg
        "QTextEdit { background: #ffffff; color: #1e1e2e; border: none; font-size: 12px; }"

        // Plain text edit
        "QPlainTextEdit { background: #ffffff; color: #1e1e2e; border: none; font-size: 12px; }"

        // Labels
        "QLabel { color: #333333; background: transparent; }"

        // Group boxes
        "QGroupBox { color: #333333; border: 1px solid #e0e0e0; border-radius: 6px;"
        " margin-top: 8px; padding-top: 4px; }"
        "QGroupBox::title { color: #555555; subcontrol-origin: margin; left: 8px; }"

        // Check boxes and radio buttons
        "QCheckBox { color: #333333; }"
        "QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #d0d0d0;"
        " border-radius: 3px; background: #ffffff; }"
        "QCheckBox::indicator:checked { background: #2563eb; border-color: #2563eb; }"
        "QRadioButton { color: #333333; }"

        // Spin boxes
        "QSpinBox, QDoubleSpinBox { background: #ffffff; color: #333333;"
        " border: 1px solid #e0e0e0; border-radius: 4px; padding: 3px 6px; }"
        "QSpinBox:focus, QDoubleSpinBox:focus { border-color: #2563eb; }"

        // Sliders
        "QSlider::groove:horizontal { background: #e0e0e0; height: 4px; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #2563eb; width: 14px; height: 14px;"
        " border-radius: 7px; margin: -5px 0; }"
        "QSlider::sub-page:horizontal { background: #2563eb; border-radius: 2px; }"

        // Progress bars
        "QProgressBar { background: #f0f0f0; border: 1px solid #e0e0e0; border-radius: 4px;"
        " color: #333333; text-align: center; }"
        "QProgressBar::chunk { background: #2563eb; border-radius: 3px; }"

        // Dialogs
        "QDialog { background: #ffffff; }"
        "QDialog QLabel { color: #333333; }"

        // Stacked widget / frames
        "QFrame { background: transparent; }"

        // Scroll areas
        "QScrollArea { background: #fafafa; border: none; }"
        "QScrollArea > QWidget > QWidget { background: #fafafa; }"

        // Tab widget pane
        "QTabWidget::pane { background: #ffffff; border: 1px solid #e0e0e0; }"

        // Table views
        "QTableView { background: #ffffff; color: #333333; border: 1px solid #e0e0e0;"
        " gridline-color: #f0f0f0; }"
        "QTableView::item:selected { background: #e0e8ff; color: #1e1e2e; }"

        // List views
        "QListView { background: #ffffff; color: #333333; border: 1px solid #e0e0e0; }"
        "QListView::item:hover { background: #f5f5f5; }"
        "QListView::item:selected { background: #e0e8ff; color: #1e1e2e; }"
    );
}

} // namespace Theme
