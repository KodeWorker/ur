#ifndef SRC_SCREEN_OPTION_HPP_
#define SRC_SCREEN_OPTION_HPP_
#include "client/base.hpp"

namespace ur::screen {
class Option {
public:
  Option();
  void Logic(GameScreen &currentScreen);
  void Display();
};
} // namespace ur::screen

#endif // SRC_SCREEN_OPTION_HPP_
