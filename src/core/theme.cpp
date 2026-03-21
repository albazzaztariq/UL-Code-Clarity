#include "core/theme.h"

// Static member definition — default to dark (Catppuccin Mocha).
// MainWindow::applyTheme() updates this before rebuilding any widget stylesheets.
bool Theme::Colors::isDark = true;
