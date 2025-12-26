#include "client/screen/option.hpp"
#include <raylib.h>

namespace ur::screen {

Option::Option() {}

void Option::Logic(GameScreen &currentScreen) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    currentScreen = MENU;
  }
}

void Option::Display() {
  DrawText("OPTIONS SCREEN", 20, 20, 40, DARKBLUE);
  DrawText("Press ESC to return to Menu", 20, 80, 20, DARKGRAY);
}

} // namespace ur::screen
