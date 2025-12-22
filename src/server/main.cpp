#include <enet/enet.h>
#include <iostream>
#include <string>
#include <flatbuffers/flatbuffers.h>
#include "player_generated.h"

using namespace ur::fbs;

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
    ENetHost* server = enet_host_create(&address, 32, 2, 0, 0);
    if (server == nullptr) {
        std::cerr << "An error occurred while trying to create an ENet server host." << std::endl;
        return 1;
    }

    std::cout << "Server started on port " << address.port << std::endl;

    ENetEvent event;
    while (true) {
        // Wait up to 1000ms for an event
        while (enet_host_service(server, &event, 1000) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    std::cout << "A new client connected from " << event.peer->address.host << ":" << event.peer->address.port << std::endl;
                    event.peer->data = (void*)"Client User";
                    break;

                case ENET_EVENT_TYPE_RECEIVE:
                    std::cout << "Packet received: " << (char*)event.packet->data << " on channel " << (int)event.channelID << std::endl;
                    // Clean up the packet
                    enet_packet_destroy(event.packet);
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                    std::cout << "Client disconnected." << std::endl;
                    event.peer->data = NULL;
                    break;
            }
        }
    }

    enet_host_destroy(server);
    return 0;
}