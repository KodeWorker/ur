#include "server/connect.hpp"
#include "server/engine.hpp"
#include <enet/enet.h>
#include <iostream>
#include <string>

int main() {

  if (enet_initialize() != 0) {
    std::cerr << "An error occurred while initializing ENet." << std::endl;
    return 1;
  }
  atexit(enet_deinitialize);

  ENetAddress address;
  address.host = ENET_HOST_ANY;
  address.port = 9487;

  // Create a server host
  ENetHost *server = enet_host_create(&address, 32, 2, 0, 0);
  if (server == nullptr) {
    std::cerr << "An error occurred while trying to create an ENet server host."
              << std::endl;
    return 1;
  }
  std::cout << "Server started on port " << address.port << std::endl;

  ur::network::ConnectHandler connectHandler;

  ENetEvent event;
  while (true) {
    // Wait up to 10ms for an event
    while (enet_host_service(server, &event, 10) > 0) {
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

  enet_host_destroy(server);
  return 0;
}
