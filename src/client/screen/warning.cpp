#include "client/screen/warning.hpp"
#include <raylib.h>

namespace ur::screen {
Warning::Warning() {}

void Warning::Logic(GameScreen &currentScreen) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    currentScreen = MENU;
  }
}

void Warning::Display(const std::string &message) {
  DrawText("WARNING", 20, 20, 40, DARKBLUE);
  DrawText(message.c_str(), 20, 80, 20, RED);
  DrawText("Press ESC to return to Menu", 20, 140, 20, DARKGRAY);
}

} // namespace ur::screen
