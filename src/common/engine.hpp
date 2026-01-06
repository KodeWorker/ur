#ifndef SRC_COMMON_ENGINE_HPP
#define SRC_COMMON_ENGINE_HPP

#include <enet/enet.h>
#include <iostream>

namespace ur::engine {

ENetPacket *GameLoop(ENetPacket *packet, uint32_t clientId) {
  // TODO: Implement game logic here
  std::string receivedMessage((char *)packet->data);
  std::cout << "[Engine] Authenticated client id [" << clientId
            << "] says: " << receivedMessage << std::endl;

  // Create a response packet
  std::string response = "Server received: " + receivedMessage;
  return enet_packet_create(response.c_str(), response.length() + 1,
                            ENET_PACKET_FLAG_RELIABLE);
}

} // namespace ur::engine

#endif // SRC_COMMON_ENGINE_HPP
