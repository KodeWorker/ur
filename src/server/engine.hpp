#ifndef SRC_SERVER_ENGINE_HPP
#define SRC_SERVER_ENGINE_HPP

#include <enet/enet.h>
#include <iostream>

namespace ur::engine {

void GameLoop(ENetPacket *packet, uint32_t clientId) {
  // TODO: Implement game logic here
  std::string receivedMessage((char *)packet->data);
  std::cout << "Authenticated client id [" << clientId
            << "] says: " << receivedMessage << std::endl;
}

} // namespace ur::engine

#endif // SRC_SERVER_ENGINE_HPP
