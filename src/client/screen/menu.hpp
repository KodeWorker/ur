#ifndef SRC_SCREEN_MENU_HPP_
#define SRC_SCREEN_MENU_HPP_
#include "client/base.hpp"
#include <raylib.h>

namespace ur::screen {

class Menu {
public:
  Menu(ScreenSettings screenSettings);
  bool shouldQuit;
  void Logic(const Vector2 &mousePoint, GameScreen &currentScreen);
  void Display(const Vector2 &mousePoint, const ScreenSettings &screenSettings);

private:
  Rectangle btnOffline;
  Rectangle btnOnline;
  Rectangle btnOptions;
  Rectangle btnQuit;
};

} // namespace ur::screen
#endif // SRC_SCREEN_MENU_HPP_
