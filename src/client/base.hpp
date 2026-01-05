#ifndef SRC_BASE_HPP_
#define SRC_BASE_HPP_
#include <enet/enet.h>
#include <string>

typedef enum GameScreen {
  MENU,
  CONNECT,
  GAME,
  OFFLINE,
  OPTIONS,
  WARNING
} GameScreen;

struct ScreenSettings {
  int width;
  int height;
  const char *title;

  ScreenSettings(int w, int h, const char *t) : width(w), height(h), title(t) {}
};

#endif // SRC_BASE_HPP_
