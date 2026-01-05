#include "common/base.hpp"
#include "common/engine.hpp"
#include "server/connect.hpp"
#include <enet/enet.h>
#include <iostream>
#include <string>

int main() {

  if (enet_initialize() != 0) {
    std::cerr << "An error occurred while initializing ENet." << std::endl;
    return 1;
  }
  atexit(enet_deinitialize);
  // Create a server host
  ENetElements enetElements;
  enetElements.address.host = ENET_HOST_ANY;
  enetElements.address.port = 9487;
  enetElements.host = enet_host_create(&enetElements.address, 32, 2, 0, 0);
  if (enetElements.host == nullptr) {
    std::cerr << "An error occurred while trying to create an ENet server host."
              << std::endl;
    return 1;
  }
  std::cout << "Server started on port " << enetElements.address.port
            << std::endl;

  ur::network::ConnectHandler connectHandler;

  ENetEvent event;
  while (true) {
    // Wait up to 10ms for an event
    while (enet_host_service(enetElements.host, &event, 10) > 0) {
      switch (event.type) {
      case ENET_EVENT_TYPE_CONNECT:
        connectHandler.ClientConnected(event);
        break;
      case ENET_EVENT_TYPE_RECEIVE:
        if ((ur::network::PeerState)(uintptr_t)event.peer->data ==
            ur::network::PeerState::CONNECTED) {
          connectHandler.GameLogin(event);
        } else if ((ur::network::PeerState)(uintptr_t)event.peer->data ==
                   ur::network::PeerState::AUTHENTICATED) {
          ur::engine::GameLoop(event.packet,
                               connectHandler.GetConnectedPeers()[event.peer]);
        }
        enet_packet_destroy(event.packet);
        break;
      case ENET_EVENT_TYPE_DISCONNECT:
        connectHandler.ClientDisconnect(event);
        break;
      }
    }
  }

  enet_host_destroy(enetElements.host);
  return 0;
}
