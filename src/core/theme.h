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

// ── Dynamic theme colors — switch with Colors::isDark ────────────────────────
// Set Theme::Colors::isDark before building any widget to pick dark/light values.
// Usage: QString ss = QString("background: %1;").arg(Theme::Colors::bg());
struct Colors {
    static bool isDark; // set by MainWindow::applyTheme
    static const char* bg()     { return isDark ? "#1e1e2e" : "#ffffff"; }
    static const char* bg2()    { return isDark ? "#2a2a3c" : "#f5f5f5"; }
    static const char* bg3()    { return isDark ? "#333348" : "#e8e8e8"; }
    static const char* bg4()    { return isDark ? "#3c3c54" : "#e0e0e0"; }
    static const char* fg()     { return isDark ? "#cdd6f4" : "#1e1e2e"; }
    static const char* fg2()    { return isDark ? "#a6adc8" : "#555555"; }
    static const char* fg3()    { return isDark ? "#6c7086" : "#999999"; }
    static const char* accent() { return isDark ? "#89b4fa" : "#2563eb"; }
    static const char* border() { return isDark ? "#45475a" : "#d0d0d0"; }
    static const char* green()  { return isDark ? "#a6e3a1" : "#22c55e"; }
    static const char* red()    { return isDark ? "#f38ba8" : "#dc2626"; }
    static const char* yellow() { return isDark ? "#f9e2af" : "#d97706"; }
};

// Unified theme stylesheet — pass isDark=true for Catppuccin Mocha, false for light
inline QString themeStyleSheet(bool isDark) {
    // Color variables selected per theme
    const char* cBase        = isDark ? "#1e1e2e" : "#ffffff";
    const char* cFg          = isDark ? "#cdd6f4" : "#333333";
    const char* cPanel       = isDark ? "#2a2a3c" : "#f0f0f0";
    const char* cPanelFg     = isDark ? "#a6adc8" : "#333333";
    const char* cBorder      = isDark ? "#313244" : "#e0e0e0";
    const char* cBorder2     = isDark ? "#45475a" : "#d0d0d0";
    const char* cHover       = isDark ? "#313244" : "#e0e0e0";
    const char* cHoverFg     = isDark ? "#cdd6f4" : "#1e1e2e";
    const char* cHover2      = isDark ? "#3c3c54" : "#e4e4e4";  // button/item hover
    const char* cPressed     = isDark ? "#45475a" : "#d8d8d8";
    const char* cStatusBg    = isDark ? "#181825" : "#f0f0f0";
    const char* cStatusFg    = isDark ? "#a6adc8" : "#555555";
    const char* cMenuBg      = isDark ? "#2a2a3c" : "#ffffff";
    const char* cMenuFg      = isDark ? "#a6adc8" : "#1e1e2e";
    const char* cMenuSelBg   = isDark ? "#e0e8ff" : "#e0e8ff";  // same both — overridden below
    const char* cScrollTrack = isDark ? "transparent" : "#f0f0f0";
    const char* cScrollThumb = isDark ? "#45475a"    : "#cccccc";
    const char* cScrollHover = isDark ? "#585b70"    : "#aaaaaa";
    const char* cScrollPage  = isDark ? "transparent": "#f0f0f0";
    const char* cAccent      = isDark ? "#89b4fa"    : "#2563eb";
    const char* cInput       = isDark ? "#2a2a3c"    : "#ffffff";
    const char* cInputFg     = isDark ? "#cdd6f4"    : "#333333";
    const char* cTabInactive = isDark ? "#2a2a3c"    : "#f0f0f0";
    const char* cTabInactFg  = isDark ? "#a6adc8"    : "#555555";
    const char* cTabActive   = isDark ? "#1e1e2e"    : "#ffffff";
    const char* cTabActiveFg = isDark ? "#cdd6f4"    : "#1e1e2e";
    const char* cTabHoverBg  = isDark ? "#313244"    : "#e8e8e8";
    const char* cTabHoverFg  = isDark ? "#cdd6f4"    : "#333333";
    const char* cTabBorder   = isDark ? "#313244"    : "transparent";
    const char* cTree        = isDark ? "#2a2a3c"    : "#fafafa";
    const char* cTreeFg      = isDark ? "#a6adc8"    : "#333333";
    const char* cTreeHover   = isDark ? "#313244"    : "#f0f0f0";
    const char* cTreeSelFg   = isDark ? "#cdd6f4"    : "#2563eb";
    const char* cHeaderBg    = isDark ? "#2a2a3c"    : "#fafafa";
    const char* cHeaderFg    = isDark ? "#a6adc8"    : "#333333";
    const char* cTextEdit    = isDark ? "#1e1e2e"    : "#ffffff";
    const char* cTextEditFg  = isDark ? "#cdd6f4"    : "#1e1e2e";
    const char* cDialogBg    = isDark ? "#1e1e2e"    : "#ffffff";
    const char* cDialogFg    = isDark ? "#cdd6f4"    : "#333333";
    const char* cArrow       = isDark ? "#cdd6f4"    : "#333333";
    const char* cMenuItemSel = isDark ? "#3c3c54"    : "#e0e8ff";
    const char* cBtnBg       = isDark ? "transparent": "#f0f0f0";
    const char* cBtnDisFg    = isDark ? "#45475a"    : "#aaaaaa";
    const char* cBtnDisBg    = isDark ? "transparent": "#f0f0f0";
    const char* cScrollArea  = isDark ? "#1e1e2e"    : "#fafafa";
    const char* cTabPane     = isDark ? "#1e1e2e"    : "#ffffff";
    const char* cLabelFg     = isDark ? "#cdd6f4"    : "#333333";

    QString ss;
    ss.reserve(4096);

    // Base
    ss += QString("QMainWindow, QWidget { background: %1; color: %2;"
                  " font-family: 'Segoe UI'; font-size: 12px; }").arg(cBase, cFg);

    // Menu bar
    ss += QString("QMenuBar { background: %1; color: %2;"
                  " border-bottom: 1px solid %3; font-size: 12px; padding: 1px 0; }")
              .arg(cPanel, cPanelFg, cBorder);
    ss += "QMenuBar::item { padding: 4px 8px; border-radius: 4px; }";
    ss += QString("QMenuBar::item:selected { background: %1; color: %2; }")
              .arg(cHover, cHoverFg);

    // Dropdown menus
    ss += QString("QMenu { background: %1; color: %2;"
                  " border: 1px solid %3; padding: 4px 0; border-radius: 6px; }")
              .arg(cMenuBg, cMenuFg, cBorder2);
    ss += "QMenu::item { padding: 5px 16px 5px 12px; }";
    ss += QString("QMenu::item:selected { background: %1; color: %2; border-radius: 3px; }")
              .arg(cMenuItemSel, cHoverFg);
    ss += QString("QMenu::separator { height: 1px; background: %1; margin: 3px 8px; }")
              .arg(cBorder);

    // Status bar
    ss += QString("QStatusBar { background: %1; color: %2;"
                  " border-top: 1px solid %3; font-size: 11px; padding: 0 4px; }")
              .arg(cStatusBg, cStatusFg, cBorder);
    ss += "QStatusBar::item { border: none; }";

    // Splitter handles
    ss += QString("QSplitter::handle { background: %1; }").arg(cBorder);
    ss += "QSplitter::handle:horizontal { width: 1px; }";
    ss += "QSplitter::handle:vertical { height: 1px; }";
    ss += QString("QSplitter::handle:hover { background: %1; }").arg(cBorder2);

    // Scrollbars
    ss += QString("QScrollBar:vertical { background: %1; width: 8px; margin: 0; }").arg(cScrollTrack);
    ss += QString("QScrollBar::handle:vertical { background: %1; border-radius: 4px;"
                  " min-height: 24px; margin: 2px; }").arg(cScrollThumb);
    ss += QString("QScrollBar::handle:vertical:hover { background: %1; }").arg(cScrollHover);
    ss += "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }";
    ss += QString("QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: %1; }").arg(cScrollPage);
    ss += QString("QScrollBar:horizontal { background: %1; height: 8px; margin: 0; }").arg(cScrollTrack);
    ss += QString("QScrollBar::handle:horizontal { background: %1; border-radius: 4px;"
                  " min-width: 24px; margin: 2px; }").arg(cScrollThumb);
    ss += QString("QScrollBar::handle:horizontal:hover { background: %1; }").arg(cScrollHover);
    ss += "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }";
    ss += QString("QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: %1; }").arg(cScrollPage);

    // Tooltip
    ss += QString("QToolTip { background: %1; color: %2; border: 1px solid %3;"
                  " padding: 4px 8px; font-size: 11px; border-radius: 4px; }")
              .arg(cMenuBg, cHoverFg, cBorder2);

    // Push buttons
    ss += QString("QPushButton { background: %1; color: %2; border: none;"
                  " border-radius: 6px; padding: 6px 16px; font-size: 12px; }")
              .arg(cBtnBg, cFg);
    ss += QString("QPushButton:hover { background: %1; }").arg(cHover2);
    ss += QString("QPushButton:pressed { background: %1; }").arg(cPressed);
    ss += QString("QPushButton:disabled { color: %1; background: %2; }")
              .arg(cBtnDisFg, cBtnDisBg);

    // Combo boxes
    ss += QString("QComboBox { background: %1; color: %2; border: 1px solid %3;"
                  " border-radius: 4px; padding: 4px 24px 4px 8px; font-size: 11px; }")
              .arg(cInput, cInputFg, cBorder2);
    ss += QString("QComboBox:focus { border-color: %1; }").arg(cAccent);
    ss += "QComboBox::drop-down { border: none; width: 20px;"
          " subcontrol-position: right; subcontrol-origin: padding; }";
    ss += QString("QComboBox::down-arrow { image: none; width: 0; height: 0;"
                  " border-left: 5px solid transparent; border-right: 5px solid transparent;"
                  " border-top: 6px solid %1; }").arg(cArrow);
    ss += QString("QComboBox QAbstractItemView { background: %1; color: %2;"
                  " border: none; outline: none; margin: 0; padding: 0;"
                  " selection-background-color: %3; }")
              .arg(cInput, cInputFg, cMenuItemSel);
    ss += "QComboBox QAbstractItemView::item { border: none; padding: 4px 8px; }";
    ss += "QComboBox QFrame { border: none; }";

    // Line edits
    ss += QString("QLineEdit { background: %1; color: %2; border: 1px solid %3;"
                  " border-radius: 4px; padding: 5px 10px; font-size: 12px; }")
              .arg(cInput, cInputFg, cBorder2);
    ss += QString("QLineEdit:focus { border-color: %1; }").arg(cAccent);

    // Tab bar
    ss += QString("QTabBar::tab { background: %1; color: %2; padding: 7px 16px;"
                  " border-right: 1px solid %3; font-size: 11px;"
                  " border-bottom: 2px solid transparent; }")
              .arg(cTabInactive, cTabInactFg, cTabBorder);
    ss += QString("QTabBar::tab:selected { background: %1; color: %2;"
                  " border-bottom: 2px solid %3; }").arg(cTabActive, cTabActiveFg, cAccent);
    ss += QString("QTabBar::tab:hover:!selected { background: %1; color: %2; }")
              .arg(cTabHoverBg, cTabHoverFg);
    ss += "QTabBar::close-button { subcontrol-position: right; }";
    if (!isDark)
        ss += "QTabBar::close-button:hover { background: transparent; }";

    // Tree view
    ss += QString("QTreeView { background: %1; color: %2; border: none; font-size: 12px; }")
              .arg(cTree, cTreeFg);
    ss += "QTreeView::item { padding: 4px 6px; min-height: 24px; }";
    ss += QString("QTreeView::item:hover { background: %1; }").arg(cTreeHover);
    ss += QString("QTreeView::item:selected { background: transparent; color: %1;"
                  " border-left: 2px solid %2; }").arg(cTreeSelFg, cAccent);
    ss += QString("QTreeView::branch { background: %1; }").arg(cTree);

    // Header
    ss += QString("QHeaderView::section { background: %1; color: %2;"
                  " border: none; padding: 4px 8px; font-size: 10px; }")
              .arg(cHeaderBg, cHeaderFg);
    if (!isDark)
        ss += QString("QHeaderView::section { border-bottom: 1px solid %1; }").arg(cBorder);

    // Text edit
    ss += QString("QTextEdit { background: %1; color: %2; border: none; font-size: 12px; }")
              .arg(cTextEdit, cTextEditFg);

    // Dialogs
    ss += QString("QDialog { background: %1; }").arg(cDialogBg);
    ss += QString("QDialog QLabel { color: %1; }").arg(cDialogFg);

    // Light-only extras
    if (!isDark) {
        ss += QString("QPlainTextEdit { background: %1; color: %2; border: none; font-size: 12px; }")
                  .arg(cTextEdit, cTextEditFg);
        ss += QString("QLabel { color: %1; background: transparent; }").arg(cLabelFg);
        ss += QString("QGroupBox { color: %1; border: 1px solid %2; border-radius: 6px;"
                      " margin-top: 8px; padding-top: 4px; }").arg(cFg, cBorder);
        ss += QString("QGroupBox::title { color: %1; subcontrol-origin: margin; left: 8px; }")
                  .arg(cStatusFg);
        ss += QString("QCheckBox { color: %1; }").arg(cFg);
        ss += QString("QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid %1;"
                      " border-radius: 3px; background: %2; }").arg(cBorder2, cBase);
        ss += QString("QCheckBox::indicator:checked { background: %1; border-color: %2; }")
                  .arg(cAccent, cAccent);
        ss += QString("QRadioButton { color: %1; }").arg(cFg);
        ss += QString("QSpinBox, QDoubleSpinBox { background: %1; color: %2;"
                      " border: 1px solid %3; border-radius: 4px; padding: 3px 6px; }")
                  .arg(cInput, cInputFg, cBorder);
        ss += QString("QSpinBox:focus, QDoubleSpinBox:focus { border-color: %1; }").arg(cAccent);
        ss += QString("QSlider::groove:horizontal { background: %1; height: 4px; border-radius: 2px; }").arg(cBorder);
        ss += QString("QSlider::handle:horizontal { background: %1; width: 14px; height: 14px;"
                      " border-radius: 7px; margin: -5px 0; }").arg(cAccent);
        ss += QString("QSlider::sub-page:horizontal { background: %1; border-radius: 2px; }").arg(cAccent);
        ss += QString("QProgressBar { background: %1; border: 1px solid %2; border-radius: 4px;"
                      " color: %3; text-align: center; }").arg(cPanel, cBorder, cFg);
        ss += QString("QProgressBar::chunk { background: %1; border-radius: 3px; }").arg(cAccent);
        ss += "QFrame { background: transparent; }";
        ss += QString("QScrollArea { background: %1; border: none; }").arg(cScrollArea);
        ss += QString("QScrollArea > QWidget > QWidget { background: %1; }").arg(cScrollArea);
        ss += QString("QTabWidget::pane { background: %1; border: 1px solid %2; }")
                  .arg(cTabPane, cBorder);
        ss += QString("QTableView { background: %1; color: %2; border: 1px solid %3;"
                      " gridline-color: %4; }").arg(cBase, cFg, cBorder, cPanel);
        ss += QString("QTableView::item:selected { background: %1; color: %2; }")
                  .arg(cMenuItemSel, cHoverFg);
        ss += QString("QListView { background: %1; color: %2; border: 1px solid %3; }")
                  .arg(cBase, cFg, cBorder);
        ss += "QListView::item:hover { background: #f5f5f5; }";
        ss += QString("QListView::item:selected { background: %1; color: %2; }")
                  .arg(cMenuItemSel, cHoverFg);
    }

    return ss;
}

// ── const char* versions for use in QString::arg() stylesheets ──────────
// Usage: #include "core/theme.h" then 'using namespace Theme::Css;'
namespace Css {
    // Backgrounds
    inline constexpr const char* BG      = "#1e1e2e";
    inline constexpr const char* BG2     = "#2a2a3c";
    inline constexpr const char* BG3     = "#333348";
    inline constexpr const char* BG4     = "#3c3c54";
    inline constexpr const char* BG5     = "#181825";

    // Foregrounds
    inline constexpr const char* FG      = "#cdd6f4";
    inline constexpr const char* FG2     = "#a6adc8";
    inline constexpr const char* FG3     = "#6c7086";

    // Accents
    inline constexpr const char* ACCENT  = "#89b4fa";
    inline constexpr const char* ACCENT2 = "#74c7ec";
    inline constexpr const char* GREEN   = "#a6e3a1";
    inline constexpr const char* RED     = "#f38ba8";
    inline constexpr const char* YELLOW  = "#f9e2af";
    inline constexpr const char* MAUVE   = "#cba6f7";
    inline constexpr const char* PEACH   = "#fab387";
    inline constexpr const char* TEAL    = "#94e2d5";

    // Border
    inline constexpr const char* BORDER  = "#45475a";

    // Light theme equivalents
    inline constexpr const char* L_BG     = "#e8e8e8";
    inline constexpr const char* L_FG     = "#1e1e2e";
    inline constexpr const char* L_FG2    = "#555555";
    inline constexpr const char* L_BORDER = "#d0d0d0";
} // namespace Css

} // namespace Theme
