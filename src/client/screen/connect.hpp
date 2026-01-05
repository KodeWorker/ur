#ifndef SRC_SCREEN_CONNECT_HPP_
#define SRC_SCREEN_CONNECT_HPP_
#include "client/base.hpp"
#include "common/base.hpp"
#include <raylib.h>
#include <string>

namespace ur::screen {

class Connect {

public:
  Connect(ScreenSettings screenSettings);
  std::string hostInput;
  std::string usernameInput;
  std::string passwordInput;
  int port = 9487; // Default port for connection to server
  void Logic(const Vector2 &mousePoint, ENetElements &enetElements,
             GameScreen &currentScreen);
  void Display(const Vector2 &mousePoint, const ScreenSettings &screenSettings);

private:
  int activeField = -1;
  int maxLength = 32;
  Rectangle hostBox;
  Rectangle usernameBox;
  Rectangle passwordBox;
  Rectangle btnConnect;
  Rectangle btnBack;
};
} // namespace ur::screen

#endif // SRC_SCREEN_CONNECT_HPP_
