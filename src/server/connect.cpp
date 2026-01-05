#include "server/connect.hpp"
#include "fbs/client_generated.h"
#include "fbs/player_generated.h"
#include <enet/enet.h>
#include <flatbuffers/flatbuffers.h>
#include <iostream>

namespace ur::network {

ConnectHandler::ConnectHandler() : connectedClients(0) {}

void ConnectHandler::ClientConnected(ENetEvent &event) {
  char hostIP[16];
  enet_address_get_host_ip(&event.peer->address, hostIP, sizeof(hostIP));
  std::cout << "Client connected from " << hostIP << ":"
            << std::to_string(ENET_NET_TO_HOST_16(event.peer->address.port))
            << std::endl;
  event.peer->data = (void *)PeerState::CONNECTED;
}

bool ConnectHandler::AuthenticateClient(const std::string &username,
                                        const std::string &password) {
  // Simple hardcoded authentication for demonstration
  return (username == "user" && password == "pass");
}

bool ConnectHandler::GameLogin(ENetEvent &event) {
  // Parse flatbuffer Client object from enet packet
  auto client = ur::fbs::GetClient(event.packet->data);

  // Verify the flatbuffer
  flatbuffers::Verifier verifier(event.packet->data, event.packet->dataLength);
  if (!client->Verify(verifier)) {
    std::cerr << "Invalid flatbuffer received" << std::endl;
    event.peer->data = (void *)PeerState::DISCONNECTED;
    enet_packet_destroy(event.packet);
    return false;
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
    auto response = ur::fbs::CreateClientDirect(
        builder, clientId, client->username()->c_str(),
        client->password()->c_str(),
        ur::fbs::Status::Status_AUTHENTICATED_SUCCESS);
    builder.Finish(response);

    ENetPacket *packet =
        enet_packet_create(builder.GetBufferPointer(), builder.GetSize(),
                           ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(event.peer, 0, packet);
    std::cout << "Client authenticated." << std::endl;
    return true;
  } else {
    event.peer->data = (void *)PeerState::DISCONNECTED;

    // Create a new flatbuffer response with failure status
    flatbuffers::FlatBufferBuilder builder;
    auto response = ur::fbs::CreateClientDirect(
        builder, 0, client->username()->c_str(), client->password()->c_str(),
        ur::fbs::Status::Status_AUTHENTICATED_FAIL);
    builder.Finish(response);

    ENetPacket *packet =
        enet_packet_create(builder.GetBufferPointer(), builder.GetSize(),
                           ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(event.peer, 0, packet);
    event.peer->data = (void *)PeerState::DISCONNECTED;
    std::cout << "Client failed to authenticate." << std::endl;
    return false;
  }
}

void ConnectHandler::ClientDisconnect(ENetEvent &event) {
  std::unique_lock lock(peersMutex);
  auto it = connectedPeers.find(event.peer);
  if (it != connectedPeers.end()) {
    std::cout << "Client id [" << it->second << "] disconnected." << std::endl;
    connectedPeers.erase(it);
  } else {
    std::cout << "Client disconnected." << std::endl;
  }
  event.peer->data = nullptr;
}
} // namespace ur::network
