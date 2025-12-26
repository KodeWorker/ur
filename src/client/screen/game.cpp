#include "client/screen/game.hpp"
#include "fbs/client_generated.h"
#include "fbs/player_generated.h"
#include <flatbuffers/flatbuffers.h>

namespace ur::screen {

Game::Game() {}

void Game::Logic(const Vector2 &mousePoint, ENetElements &enetElements,
                 GameScreen &currentScreen) {
  // 1. Networking (Non-blocking poll)
  while (enet_host_service(enetElements.client, &enetElements.event, 0) > 0) {
    switch (enetElements.event.type) {
    case ENET_EVENT_TYPE_CONNECT:
      TraceLog(LOG_INFO, "Successfully connected to server");
      break;
    case ENET_EVENT_TYPE_RECEIVE: {
      auto fbsClient = ur::fbs::GetClient(enetElements.event.packet->data);
      // Verify the flatbuffer
      flatbuffers::Verifier verifier(enetElements.event.packet->data,
                                     enetElements.event.packet->dataLength);
      if (!fbsClient->Verify(verifier)) {
        TraceLog(LOG_WARNING, "Invalid flatbuffer received");
      } else {
        if (fbsClient->status() ==
            ur::fbs::Status::Status_AUTHENTICATED_SUCCESS) {
          TraceLog(LOG_INFO, "Authenticated by server as user: %s with ID: %d",
                   fbsClient->username()->c_str(), fbsClient->id());
        } else if (fbsClient->status() ==
                   ur::fbs::Status::Status_AUTHENTICATED_FAIL) {
          enet_peer_disconnect(enetElements.peer, 0);
          enet_peer_reset(enetElements.peer);
          enetElements.peer = nullptr;
          enetElements.warningMessage = "Authentication failed";
          currentScreen = WARNING;
          TraceLog(LOG_WARNING, "Authentication failed for user: %s",
                   fbsClient->username()->c_str());
        } else {
          TraceLog(LOG_INFO, "Received message from server for user: %s",
                   fbsClient->username()->c_str());
        }
      }
      enet_packet_destroy(enetElements.event.packet);
      break;
    }
    case ENET_EVENT_TYPE_DISCONNECT: {
      enetElements.peer = nullptr;
      enetElements.warningMessage = "Disconnected from server";
      currentScreen = WARNING;
      TraceLog(LOG_WARNING, "Disconnected from server");
      break;
    }
    }
  }

  // 2. Input
  if (IsKeyPressed(KEY_SPACE)) {
    if (enetElements.peer != nullptr) {
      SendMessage(enetElements.peer, "Hello Server!");
      TraceLog(LOG_INFO, "Sent message: %s", "Hello Server!");
    }
  } else if (IsKeyPressed(KEY_ESCAPE)) {
    if (enetElements.peer != nullptr) {
      enet_peer_disconnect(enetElements.peer, 0);
      enet_peer_reset(enetElements.peer);
      enetElements.peer = nullptr;
      TraceLog(LOG_INFO, "Disconnecting from server...");
    }
    currentScreen = MENU;
  }
}

void Game::Display() {
  DrawText("GAME SCREEN", 20, 20, 40, MAROON);
  DrawText("Press SPACE to send a \"Hello Server!\" message", 20, 80, 20,
           DARKGRAY);
  DrawText("Press ESC to return to Menu", 20, 110, 20, DARKGRAY);
}

} // namespace ur::screen
