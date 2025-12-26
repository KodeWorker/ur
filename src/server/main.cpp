#include "fbs/client_generated.h"
#include "fbs/player_generated.h"
#include <enet/enet.h>
#include <flatbuffers/flatbuffers.h>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

using namespace ur::fbs;

bool AuthenticateClient(const std::string &username,
                        const std::string &password) {
  // Simple hardcoded authentication for demonstration
  return (username == "user" && password == "pass");
}

int main() {

  enum PeerState { CONNECTED, AUTHENTICATED, DISCONNECTED };

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
  std::unordered_map<ENetPeer *, uint32_t> connectedPeers;
  std::shared_mutex peersMutex;
  uint32_t connectedClients = 0; // Start client IDs from 0

  ENetEvent event;
  while (true) {
    // Wait up to 1000ms for an event
    while (enet_host_service(server, &event, 1000) > 0) {
      switch (event.type) {
      case ENET_EVENT_TYPE_CONNECT: {
        char hostIP[16];
        enet_address_get_host_ip(&event.peer->address, hostIP, sizeof(hostIP));
        std::cout << "A new client connected from " << hostIP << ":"
                  << std::to_string(
                         ENET_NET_TO_HOST_16(event.peer->address.port))
                  << std::endl;
        event.peer->data = (void *)PeerState::CONNECTED;
        break;
      }

      case ENET_EVENT_TYPE_RECEIVE:
        if ((PeerState)(uintptr_t)event.peer->data == PeerState::CONNECTED) {
          // Parse flatbuffer Client object from enet packet
          auto client = GetClient(event.packet->data);

          // Verify the flatbuffer
          flatbuffers::Verifier verifier(event.packet->data,
                                         event.packet->dataLength);
          if (!client->Verify(verifier)) {
            std::cerr << "Invalid flatbuffer received" << std::endl;
            enet_packet_destroy(event.packet);
            break;
          }

          // Simple authentication check
          if (AuthenticateClient(client->username()->str(),
                                 client->password()->str())) {
            event.peer->data = (void *)PeerState::AUTHENTICATED;
            uint32_t clientId;
            {
              std::unique_lock lock(peersMutex);
              clientId = ++connectedClients; // 0 reserved for server
              connectedPeers[event.peer] = clientId;
            }

            // Create a new flatbuffer response with updated fields
            flatbuffers::FlatBufferBuilder builder;
            auto response = CreateClientDirect(
                builder, clientId, client->username()->c_str(),
                client->password()->c_str(),
                Status::Status_AUTHENTICATED_SUCCESS);
            builder.Finish(response);

            ENetPacket *packet = enet_packet_create(builder.GetBufferPointer(),
                                                    builder.GetSize(),
                                                    ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(event.peer, 0, packet);
            std::cout << "Client authenticated." << std::endl;
          } else {
            event.peer->data = (void *)PeerState::DISCONNECTED;

            // Create a new flatbuffer response with failure status
            flatbuffers::FlatBufferBuilder builder;
            auto response = CreateClientDirect(
                builder, 0, client->username()->c_str(),
                client->password()->c_str(), Status::Status_AUTHENTICATED_FAIL);
            builder.Finish(response);

            ENetPacket *packet = enet_packet_create(builder.GetBufferPointer(),
                                                    builder.GetSize(),
                                                    ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(event.peer, 0, packet);
            event.peer->data = (void *)PeerState::DISCONNECTED;
            std::cout << "Client failed to authenticate." << std::endl;
          }
        } else if ((PeerState)(uintptr_t)event.peer->data ==
                   PeerState::AUTHENTICATED) {
          std::string receivedMessage((char *)event.packet->data);
          std::cout << "Authenticated client id [" << connectedPeers[event.peer]
                    << "] says: " << receivedMessage << std::endl;
        }
        enet_packet_destroy(event.packet);
        break;

      case ENET_EVENT_TYPE_DISCONNECT:
        std::cout << "Client disconnected." << std::endl;
        {
          std::unique_lock lock(peersMutex);
          enet_peer_disconnect(event.peer, 0);
          auto it = connectedPeers.find(event.peer);
          if (it != connectedPeers.end()) {
            connectedPeers.erase(it);
          }
        }
        break;
      }
    }
  }

  enet_host_destroy(server);
  return 0;
}
