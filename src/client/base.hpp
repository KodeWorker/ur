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

class ENetElements {
public:
  ENetHost *client = nullptr;
  ENetAddress address;
  ENetEvent event;
  ENetPeer *peer = nullptr;
  std::string warningMessage = "";

  ENetElements() {};
};

// Define the different screens/states of our game
inline void SendMessage(ENetPeer *peer, const std::string &message) {
  ENetPacket *packet = enet_packet_create(message.c_str(), message.length() + 1,
                                          ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(peer, 0, packet);
}

#endif // SRC_BASE_HPP_
