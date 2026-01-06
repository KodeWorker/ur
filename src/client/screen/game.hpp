#ifndef SRC_SCREEN_GAME_HPP_
#define SRC_SCREEN_GAME_HPP_
#include "client/base.hpp"
#include "common/base.hpp"
#include <raylib.h>

namespace ur::screen {

class Game {
public:
  Game();
  void Logic(const Vector2 &mousePoint, ENetElements &enetElements,
             GameScreen &currentScreen);
  ENetPacket *Logic(const Vector2 &mousePoint, GameScreen &currentScreen);
  void Display();
  void SetLastReceivedPacket(ENetPacket *packet) {
    if (lastReceivedPacket != nullptr) {
      enet_packet_destroy(lastReceivedPacket);
    }
    lastReceivedPacket = packet;
  }

private:
  ENetPacket *lastReceivedPacket = nullptr;
};

} // namespace ur::screen

#endif // SRC_SCREEN_GAME_HPP_
