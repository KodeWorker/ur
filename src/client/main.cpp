// #include "fbs/client_generated.h" //TODO: Is this needed?
// #include "fbs/player_generated.h" //TODO: Is this needed?
// #include <flatbuffers/flatbuffers.h> //TODO: Is this needed?
#include "client/base.hpp"
#include "client/screen/connect.hpp"
#include "client/screen/game.hpp"
#include "client/screen/menu.hpp"
#include "client/screen/option.hpp"
#include "client/screen/warning.hpp"
#include "common/base.hpp"
#include "common/engine.hpp"
#include <enet/enet.h>
#include <iostream>
#include <raylib.h>
#include <string>

int main() {
  // --- Raylib Setup ---
  ScreenSettings screenSettings(800, 600, "Game Client");
  InitWindow(screenSettings.width, screenSettings.height, screenSettings.title);
  SetExitKey(KEY_NULL); // Disable default ESC to close window behavior
  SetTargetFPS(60);

  Vector2 mousePoint = {0.0f, 0.0f};

  GameScreen currentScreen = MENU;

  ur::screen::Menu menu(screenSettings);
  ur::screen::Connect connect(screenSettings);
  ur::screen::Game game;
  ur::screen::Option option;
  ur::screen::Warning warning;

  // --- ENet Setup ---
  if (enet_initialize() != 0) {
    std::cerr << "An error occurred while initializing ENet." << std::endl;
    return 1;
  }
  atexit(enet_deinitialize);
  ENetElements enetElements;
  enetElements.host = enet_host_create(NULL, 1, 2, 0, 0);

  // --- Main Loop ---
  while (!WindowShouldClose() && !menu.shouldQuit) {
    // Update
    mousePoint = GetMousePosition();

    // Game Screen Logic
    if (currentScreen == MENU) {
      menu.Logic(mousePoint, currentScreen);
    } else if (currentScreen == CONNECT) {
      connect.Logic(mousePoint, enetElements, currentScreen);
    } else if (currentScreen == GAME) {
      game.Logic(mousePoint, enetElements, currentScreen);
    } else if (currentScreen == OFFLINE) {
      ENetPacket *packet = game.Logic(mousePoint, currentScreen);
      if (packet != nullptr)
        ur::engine::GameLoop(packet, 0); // Offline mode with clientId 0
    } else if (currentScreen == OPTIONS) {
      option.Logic(currentScreen);
    } else if (currentScreen == WARNING) {
      warning.Logic(currentScreen);
    }

    // Rendering
    BeginDrawing();
    ClearBackground(RAYWHITE);
    if (currentScreen == MENU) {
      menu.Display(mousePoint, screenSettings);
    } else if (currentScreen == GAME || currentScreen == OFFLINE) {
      game.Display();
    } else if (currentScreen == OPTIONS) {
      option.Display();
    } else if (currentScreen == CONNECT) {
      connect.Display(mousePoint, screenSettings);
    } else if (currentScreen == WARNING) {
      warning.Display(enetElements.warningMessage);
    }
    EndDrawing();
  }

  // Cleanup
  if (enetElements.peer != nullptr) {
    enet_peer_disconnect(enetElements.peer, 0);
  }
  if (enetElements.host != nullptr) {
    enet_host_destroy(enetElements.host);
  }

  CloseWindow();
  return 0;
}
