#ifndef SRC_CLIENT_FBS_HPP_
#define SRC_CLIENT_FBS_HPP_
#include "fbs/client_generated.h"
#include "fbs/player_generated.h"
#include <enet/enet.h>
#include <flatbuffers/flatbuffers.h>
#include <string>

namespace ur::fbs {
inline void SendClient(ENetPeer *peer, const std::string &username,
                       const std::string &password) {
  flatbuffers::FlatBufferBuilder builder;

  auto usernameOffset = builder.CreateString(username);
  auto passwordOffset = builder.CreateString(password);

  auto client = CreateClient(builder, 0, usernameOffset,
                             passwordOffset); // id = 0 for new client
  builder.Finish(client);

  ENetPacket *packet = enet_packet_create(
      builder.GetBufferPointer(), builder.GetSize(), ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(peer, 0, packet);
}
} // namespace ur::fbs
#endif // SRC_CLIENT_FBS_HPP_
