#ifndef SRC_SERVER_CONNECT_HPP
#define SRC_SERVER_CONNECT_HPP
#include <enet/enet.h>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace ur::network {

enum PeerState { CONNECTED, AUTHENTICATED, DISCONNECTED };

class ConnectHandler {
public:
  // Constructor
  ConnectHandler();
  void ClientConnected(ENetEvent &event);
  bool GameLogin(ENetEvent &event);
  void ClientDisconnect(ENetEvent &event);
  std::unordered_map<ENetPeer *, uint32_t> &GetConnectedPeers() {
    return connectedPeers;
  }

private:
  std::unordered_map<ENetPeer *, uint32_t> connectedPeers;
  std::shared_mutex peersMutex;
  uint32_t connectedClients = 0;
  bool AuthenticateClient(const std::string &username,
                          const std::string &password);
};

} // namespace ur::network
#endif // SRC_SERVER_CONNECT_HPP
