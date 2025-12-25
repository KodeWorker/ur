#include <iostream>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <enet/enet.h>
#include <flatbuffers/flatbuffers.h>
#include "player_generated.h"

using namespace ur::fbs;

int main() {

    enum PeerState {
        CONNECTED,
        AUTHENTICATED,
        DISCONNECTED
    };

    if (enet_initialize() != 0) {
        std::cerr << "An error occurred while initializing ENet." << std::endl;
        return 1;
    }
    atexit(enet_deinitialize);

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 9487;

    // Create a server host
    ENetHost* server = enet_host_create(&address, 32, 2, 0, 0);
    if (server == nullptr) {
        std::cerr << "An error occurred while trying to create an ENet server host." << std::endl;
        return 1;
    }

    std::cout << "Server started on port " << address.port << std::endl;
    std::unordered_map<ENetPeer*, uint32_t> connectedPeers;
    std::shared_mutex peersMutex;

    ENetEvent event;
    while (true) {
        // Wait up to 1000ms for an event
        while (enet_host_service(server, &event, 1000) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    std::cout << "A new client connected from " << event.peer->address.host << ":" << event.peer->address.port << std::endl;
                    event.peer->data = (void*) PeerState::CONNECTED;
                    break;

                case ENET_EVENT_TYPE_RECEIVE:
                    if((PeerState)(uintptr_t)event.peer->data == PeerState::CONNECTED) {
                        std::string receivedMessage((char*)event.packet->data);
                        std::cout << "Received message: " << receivedMessage << std::endl;

                        // Simple authentication check
                        if(receivedMessage.find("LOGIN") == 0) {
                            event.peer->data = (void*) PeerState::AUTHENTICATED;
                            std::string response = "AUTH_SUCCESS";
                            ENetPacket* packet = enet_packet_create(response.c_str(), response.length() + 1, ENET_PACKET_FLAG_RELIABLE);
                            enet_peer_send(event.peer, 0, packet);
                            std::cout << "Client authenticated." << std::endl;
                            {
                                std::unique_lock lock(peersMutex);
                                connectedPeers[event.peer] = connectedPeers.size();
                            }
                        } else {
                            std::string response = "AUTH_FAILED";
                            ENetPacket* packet = enet_packet_create(response.c_str(), response.length() + 1, ENET_PACKET_FLAG_RELIABLE);
                            enet_peer_send(event.peer, 0, packet);
                            event.peer->data = (void*) PeerState::DISCONNECTED;
                            std::cout << "Client failed to authenticate." << std::endl;
                        }
                    } else if((PeerState)(uintptr_t)event.peer->data == PeerState::AUTHENTICATED) {
                        std::string receivedMessage((char*)event.packet->data);
                        std::cout << "Authenticated client says: " << receivedMessage << std::endl;
                    }
                    enet_packet_destroy(event.packet);
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                    std::cout << "Client disconnected." << std::endl;
                    {
                        std::unique_lock lock(peersMutex);
                        auto it = connectedPeers.find(event.peer);
                        if(it != connectedPeers.end()) {
                            connectedPeers.erase(it);
                        }
                    }
                    event.peer->data = (void*) PeerState::DISCONNECTED;
                    break;
            }
        }
    }

    enet_host_destroy(server);
    return 0;
}