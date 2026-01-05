#ifndef SRC_COMMON_BASE_HPP_
#define SRC_COMMON_BASE_HPP_

#include <enet/enet.h>
#include <string>

class ENetElements {
public:
  ENetHost *host = nullptr;
  ENetAddress address;
  ENetEvent event;
  ENetPeer *peer = nullptr;
  std::string warningMessage = "";

  ENetElements() {};
};

#endif // SRC_COMMON_BASE_HPP_
