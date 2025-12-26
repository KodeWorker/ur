#ifndef SRC_SCREEN_WARNING_HPP_
#define SRC_SCREEN_WARNING_HPP_
#include "client/base.hpp"
#include <string>

namespace ur::screen {
class Warning {
public:
  Warning();
  void Logic(GameScreen &currentScreen);
  void Display(const std::string &message);
};
} // namespace ur::screen

#endif // SRC_SCREEN_WARNING_HPP_
