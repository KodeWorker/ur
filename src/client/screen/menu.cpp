#include "client/screen/menu.hpp"

namespace ur::screen {

Menu::Menu(ScreenSettings screenSettings)
    : btnOffline{(float)screenSettings.width / 2 - 100, 150, 200, 50},
      btnOnline{(float)screenSettings.width / 2 - 100, 220, 200, 50},
      btnOptions{(float)screenSettings.width / 2 - 100, 290, 200, 50},
      btnQuit{(float)screenSettings.width / 2 - 100, 360, 200, 50},
      shouldQuit(false) {}

void Menu::Logic(const Vector2 &mousePoint, GameScreen &currentScreen) {
  // Check Offline Button
  if (CheckCollisionPointRec(mousePoint, btnOffline)) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
      currentScreen = OFFLINE;
  }
  // Check Online Button
  if (CheckCollisionPointRec(mousePoint, btnOnline)) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
      currentScreen = CONNECT;
  }
  // Check Options Button
  if (CheckCollisionPointRec(mousePoint, btnOptions)) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
      currentScreen = OPTIONS;
  }
  // Check Quit Button
  if (CheckCollisionPointRec(mousePoint, btnQuit)) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
      shouldQuit = true;
  }
}

void Menu::Display(const Vector2 &mousePoint,
                   const ScreenSettings &screenSettings) {
  DrawText("MAIN MENU",
           screenSettings.width / 2 - MeasureText("MAIN MENU", 40) / 2, 60, 40,
           DARKGRAY);
  // Draw Offline Button
  DrawRectangleRec(btnOffline, CheckCollisionPointRec(mousePoint, btnOffline)
                                   ? LIGHTGRAY
                                   : GRAY);
  DrawText("OFFLINE", btnOffline.x + 60, btnOffline.y + 15, 20, BLACK);

  // Draw Online Button
  DrawRectangleRec(btnOnline, CheckCollisionPointRec(mousePoint, btnOnline)
                                  ? LIGHTGRAY
                                  : GRAY);
  DrawText("ONLINE", btnOnline.x + 60, btnOnline.y + 15, 20, BLACK);

  // Draw Options Button
  DrawRectangleRec(btnOptions, CheckCollisionPointRec(mousePoint, btnOptions)
                                   ? LIGHTGRAY
                                   : GRAY);
  DrawText("OPTIONS", btnOptions.x + 50, btnOptions.y + 15, 20, BLACK);
  // Draw Quit Button
  DrawRectangleRec(btnQuit,
                   CheckCollisionPointRec(mousePoint, btnQuit) ? RED : MAROON);
  DrawText("QUIT", btnQuit.x + 70, btnQuit.y + 15, 20, WHITE);
}
} // namespace ur::screen
