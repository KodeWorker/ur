#include "client_generated.h"
#include "player_generated.h"
#include <enet/enet.h>
#include <flatbuffers/flatbuffers.h>
#include <iostream>
#include <raylib.h>
#include <string>

using namespace ur::fbs;

// Define the different screens/states of our game
typedef enum GameScreen { MENU, CONNECT, GAME, OPTIONS, WARNING } GameScreen;

void SendMessage(ENetPeer *peer, const std::string &message) {
  ENetPacket *packet = enet_packet_create(message.c_str(), message.length() + 1,
                                          ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(peer, 0, packet);
  TraceLog(LOG_INFO, "Sent message: %s", message.c_str());
}

void SendClient(ENetPeer *peer, const std::string &username,
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
  TraceLog(LOG_INFO, "Sent Client FlatBuffer packet for user: %s",
           username.c_str());
}

struct ScreenOptions {
  int width;
  int height;
  const char *title;

  ScreenOptions(int w, int h, const char *t) : width(w), height(h), title(t) {}
};

struct ENetElements {
  ENetHost *client;
  ENetAddress &address;
  ENetEvent &event;
  ENetPeer *peer;
  std::string &warningMessage;

  ENetElements(ENetHost *client_, ENetAddress &address_, ENetEvent &event_,
               ENetPeer *peer_, std::string &warningMessage_)
      : client(client_), address(address_), event(event_), peer(peer_),
        warningMessage(warningMessage_) {}
};

struct MenuScreenElements {
  bool &shouldQuit;
  Rectangle &btnOffline;
  Rectangle &btnOnline;
  Rectangle &btnOptions;
  Rectangle &btnQuit;

  MenuScreenElements(bool &shouldQuit_, Rectangle &btnOffline_,
                     Rectangle &btnOnline_, Rectangle &btnOptions_,
                     Rectangle &btnQuit_)
      : shouldQuit(shouldQuit_), btnOffline(btnOffline_), btnOnline(btnOnline_),
        btnOptions(btnOptions_), btnQuit(btnQuit_) {}
};

struct ConnectScreenElements {
  int &activeField;
  int maxLength;
  int port;
  std::string &hostInput;
  std::string &usernameInput;
  std::string &passwordInput;
  Rectangle &hostBox;
  Rectangle &usernameBox;
  Rectangle &passwordBox;
  Rectangle &btnConnect;
  Rectangle &btnBack;

  ConnectScreenElements(int &activeField_, int maxLength_, int port_,
                        std::string &hostInput_, std::string &usernameInput_,
                        std::string &passwordInput_, Rectangle &hostBox_,
                        Rectangle &usernameBox_, Rectangle &passwordBox_,
                        Rectangle &btnConnect_, Rectangle &btnBack_)
      : activeField(activeField_), maxLength(maxLength_), port(port_),
        hostInput(hostInput_), usernameInput(usernameInput_),
        passwordInput(passwordInput_), hostBox(hostBox_),
        usernameBox(usernameBox_), passwordBox(passwordBox_),
        btnConnect(btnConnect_), btnBack(btnBack_) {}
};

void MenuScreenLogic(const Vector2 &mousePoint,
                     const MenuScreenElements &menuElements,
                     GameScreen &currentScreen) {
  // Check Offline Button
  if (CheckCollisionPointRec(mousePoint, menuElements.btnOffline)) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
      currentScreen = GAME;
  }
  // Check Online Button
  if (CheckCollisionPointRec(mousePoint, menuElements.btnOnline)) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
      currentScreen = CONNECT;
  }
  // Check Options Button
  if (CheckCollisionPointRec(mousePoint, menuElements.btnOptions)) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
      currentScreen = OPTIONS;
  }
  // Check Quit Button
  if (CheckCollisionPointRec(mousePoint, menuElements.btnQuit)) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
      menuElements.shouldQuit = true;
  }
}

void DisplayMenuScreen(const Vector2 &mousePoint,
                       const MenuScreenElements &menuElements,
                       const ScreenOptions &screenOptions) {
  DrawText("MAIN MENU",
           screenOptions.width / 2 - MeasureText("MAIN MENU", 40) / 2, 60, 40,
           DARKGRAY);
  // Draw Offline Button
  DrawRectangleRec(menuElements.btnOffline,
                   CheckCollisionPointRec(mousePoint, menuElements.btnOffline)
                       ? LIGHTGRAY
                       : GRAY);
  DrawText("OFFLINE", menuElements.btnOffline.x + 60,
           menuElements.btnOffline.y + 15, 20, BLACK);

  // Draw Online Button
  DrawRectangleRec(menuElements.btnOnline,
                   CheckCollisionPointRec(mousePoint, menuElements.btnOnline)
                       ? LIGHTGRAY
                       : GRAY);
  DrawText("ONLINE", menuElements.btnOnline.x + 60,
           menuElements.btnOnline.y + 15, 20, BLACK);

  // Draw Options Button
  DrawRectangleRec(menuElements.btnOptions,
                   CheckCollisionPointRec(mousePoint, menuElements.btnOptions)
                       ? LIGHTGRAY
                       : GRAY);
  DrawText("OPTIONS", menuElements.btnOptions.x + 50,
           menuElements.btnOptions.y + 15, 20, BLACK);

  // Draw Quit Button
  DrawRectangleRec(
      menuElements.btnQuit,
      CheckCollisionPointRec(mousePoint, menuElements.btnQuit) ? RED : MAROON);
  DrawText("QUIT", menuElements.btnQuit.x + 70, menuElements.btnQuit.y + 15, 20,
           WHITE);
}

void DisplayGameScreen() {
  DrawText("GAME SCREEN", 20, 20, 40, MAROON);
  DrawText("Press SPACE to send a \"Hello Server!\" message", 20, 80, 20,
           DARKGRAY);
  DrawText("Press ESC to return to Menu", 20, 110, 20, DARKGRAY);
}

void DisplayOptionsScreen() {
  DrawText("OPTIONS SCREEN", 20, 20, 40, DARKBLUE);
  DrawText("Press ESC to return to Menu", 20, 80, 20, DARKGRAY);
}

void ConnectScreenLogic(const Vector2 &mousePoint,
                        ConnectScreenElements &connectElements,
                        GameScreen &currentScreen, ENetElements &enetElements) {
  // Handle text input for connection screen
  if (CheckCollisionPointRec(mousePoint, connectElements.hostBox) &&
      IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    connectElements.activeField = 0;
  } else if (CheckCollisionPointRec(mousePoint, connectElements.usernameBox) &&
             IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    connectElements.activeField = 1;
  } else if (CheckCollisionPointRec(mousePoint, connectElements.passwordBox) &&
             IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    connectElements.activeField = 2;
  } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
             !CheckCollisionPointRec(mousePoint, connectElements.hostBox) &&
             !CheckCollisionPointRec(mousePoint, connectElements.usernameBox) &&
             !CheckCollisionPointRec(mousePoint, connectElements.passwordBox)) {
    connectElements.activeField = -1;
  }

  // Handle keyboard input for active field
  if (connectElements.activeField >= 0) {
    std::string &currentInput =
        (connectElements.activeField == 0)   ? connectElements.hostInput
        : (connectElements.activeField == 1) ? connectElements.usernameInput
                                             : connectElements.passwordInput;
    int currentLength = currentInput.length();

    // Get character pressed
    int key = GetCharPressed();
    while (key > 0) {
      if (key >= 32 && key <= 125 &&
          currentLength < connectElements.maxLength) {
        currentInput.resize(currentLength + 1);
        currentInput[currentLength] = (char)key;
        currentLength++;
      }
      key = GetCharPressed();
    }

    // Handle backspace
    if (IsKeyPressed(KEY_BACKSPACE) && currentLength > 0) {
      currentInput.resize(currentLength - 1);
    }
  }

  // Connect button
  if (CheckCollisionPointRec(mousePoint, connectElements.btnConnect)) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      // Setup server address
      enet_address_set_host(&enetElements.address,
                            connectElements.hostInput.c_str());
      enetElements.address.port = connectElements.port;
      // Connect to server
      enetElements.peer =
          enet_host_connect(enetElements.client, &enetElements.address, 2, 0);
      // Timeout for connection attempt
      if (enet_host_service(enetElements.client, &enetElements.event, 5000) >
              0 &&
          enetElements.event.type == ENET_EVENT_TYPE_CONNECT) {
        SendClient(enetElements.peer, connectElements.usernameInput,
                   connectElements.passwordInput);
        enet_peer_ping_interval(enetElements.peer,
                                5000); // heartbeat every 5 seconds
        currentScreen = GAME;
      } else {
        // Either 5 seconds passed or we got a DISCONNECT event
        enet_peer_reset(enetElements.peer);
        enetElements.peer = nullptr;
        enetElements.warningMessage = "Connection failed - timeout";
        currentScreen = WARNING;
        TraceLog(LOG_WARNING, "Connection failed to %s:%d",
                 connectElements.hostInput.c_str(), connectElements.port);
      }
    }
  }

  // Back button
  if (CheckCollisionPointRec(mousePoint, connectElements.btnBack)) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      currentScreen = MENU;
    }
  }
}

void DisplayConnectScreen(const Vector2 &mousePoint,
                          const ConnectScreenElements &connectElements,
                          const ScreenOptions &screenOptions) {
  DrawText("CONNECT TO SERVER",
           screenOptions.width / 2 - MeasureText("CONNECT TO SERVER", 40) / 2,
           40, 40, DARKBLUE);

  // Draw Host input box
  DrawRectangleRec(connectElements.hostBox,
                   connectElements.activeField == 0 ? LIGHTGRAY : RAYWHITE);
  DrawRectangleLinesEx(connectElements.hostBox, 2,
                       connectElements.activeField == 0 ? BLUE : DARKGRAY);
  DrawText("Host:", connectElements.hostBox.x - 55,
           connectElements.hostBox.y + 10, 20, DARKGRAY);
  DrawText(connectElements.hostInput.c_str(), connectElements.hostBox.x + 5,
           connectElements.hostBox.y + 10, 20, BLACK);

  // Draw Username input box
  DrawRectangleRec(connectElements.usernameBox,
                   connectElements.activeField == 1 ? LIGHTGRAY : RAYWHITE);
  DrawRectangleLinesEx(connectElements.usernameBox, 2,
                       connectElements.activeField == 1 ? BLUE : DARKGRAY);
  DrawText("Username:", connectElements.usernameBox.x - 110,
           connectElements.usernameBox.y + 10, 20, DARKGRAY);
  DrawText(connectElements.usernameInput.c_str(),
           connectElements.usernameBox.x + 5,
           connectElements.usernameBox.y + 10, 20, BLACK);

  // Draw Password input box
  DrawRectangleRec(connectElements.passwordBox,
                   connectElements.activeField == 2 ? LIGHTGRAY : RAYWHITE);
  DrawRectangleLinesEx(connectElements.passwordBox, 2,
                       connectElements.activeField == 2 ? BLUE : DARKGRAY);
  DrawText("Password:", connectElements.passwordBox.x - 110,
           connectElements.passwordBox.y + 10, 20, DARKGRAY);

  // Display password as asterisks
  std::string maskedPassword(connectElements.passwordInput.length(), '*');
  DrawText(maskedPassword.c_str(), connectElements.passwordBox.x + 5,
           connectElements.passwordBox.y + 10, 20, BLACK);

  // Draw Connect button
  DrawRectangleRec(
      connectElements.btnConnect,
      CheckCollisionPointRec(mousePoint, connectElements.btnConnect) ? DARKGREEN
                                                                     : GREEN);
  DrawText("CONNECT", connectElements.btnConnect.x + 50,
           connectElements.btnConnect.y + 15, 20, WHITE);

  // Draw Back button
  DrawRectangleRec(connectElements.btnBack,
                   CheckCollisionPointRec(mousePoint, connectElements.btnBack)
                       ? LIGHTGRAY
                       : GRAY);
  DrawText("BACK", connectElements.btnBack.x + 70,
           connectElements.btnBack.y + 15, 20, BLACK);
}

void GameScreenLogic(const Vector2 &mousePoint, GameScreen &currentScreen,
                     ENetElements &enetElements) {
  // 1. Networking (Non-blocking poll)
  while (enet_host_service(enetElements.client, &enetElements.event, 0) > 0) {
    switch (enetElements.event.type) {
    case ENET_EVENT_TYPE_CONNECT:
      TraceLog(LOG_INFO, "Successfully connected to server");
      break;
    case ENET_EVENT_TYPE_RECEIVE: {
      auto fbsClient = GetClient(enetElements.event.packet->data);
      // Verify the flatbuffer
      flatbuffers::Verifier verifier(enetElements.event.packet->data,
                                     enetElements.event.packet->dataLength);
      if (!fbsClient->Verify(verifier)) {
        std::cerr << "Invalid flatbuffer received" << std::endl;
      } else {
        if (fbsClient->status() == Status::Status_AUTHENTICATED_SUCCESS) {
          TraceLog(LOG_INFO, "Authenticated by server as user: %s with ID: %d",
                   fbsClient->username()->c_str(), fbsClient->id());
        } else if (fbsClient->status() == Status::Status_AUTHENTICATED_FAIL) {
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
    case ENET_EVENT_TYPE_DISCONNECT:
      enetElements.peer = nullptr;
      enetElements.warningMessage = "Disconnected from server";
      currentScreen = WARNING;
      TraceLog(LOG_WARNING, "Disconnected from server");
      break;
    }
  }

  // 2. Input
  if (IsKeyPressed(KEY_SPACE)) {
    if (enetElements.peer != nullptr) {
      SendMessage(enetElements.peer, "Hello Server!");
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

void DisplayWarningScreen(const std::string &message) {
  DrawText("WARNING", 20, 20, 40, DARKBLUE);
  DrawText(message.c_str(), 20, 80, 20, RED);
  DrawText("Press ESC to return to Menu", 20, 140, 20, DARKGRAY);
}

int main() {
  // --- Raylib Setup ---
  ScreenOptions screenOptions(800, 600, "Game Client");
  InitWindow(screenOptions.width, screenOptions.height, screenOptions.title);
  SetExitKey(KEY_NULL); // Disable default ESC to close window behavior
  SetTargetFPS(60);

  Vector2 mousePoint = {0.0f, 0.0f};

  GameScreen currentScreen = MENU;

  // Define button rectangles
  Rectangle btnOffline = {(float)screenOptions.width / 2 - 100, 150, 200, 50};
  Rectangle btnOnline = {(float)screenOptions.width / 2 - 100, 220, 200, 50};
  Rectangle btnOptions = {(float)screenOptions.width / 2 - 100, 290, 200, 50};
  Rectangle btnQuit = {(float)screenOptions.width / 2 - 100, 360, 200, 50};
  bool shouldQuit = false;
  MenuScreenElements menuElements(shouldQuit, btnOffline, btnOnline, btnOptions,
                                  btnQuit);

  // Connection input fields
  std::string hostInput = "127.0.0.1";
  std::string usernameInput = "";
  std::string passwordInput = "";
  int activeField = -1; // -1: none, 0: host, 1: username, 2: password
  const int maxLength = 32;
  const int port = 9487;
  Rectangle hostBox = {(float)screenOptions.width / 2 - 150, 120, 300, 40};
  Rectangle usernameBox = {(float)screenOptions.width / 2 - 150, 180, 300, 40};
  Rectangle passwordBox = {(float)screenOptions.width / 2 - 150, 240, 300, 40};
  Rectangle btnConnect = {(float)screenOptions.width / 2 - 100, 300, 200, 50};
  Rectangle btnBack = {(float)screenOptions.width / 2 - 100, 370, 200, 50};
  ConnectScreenElements connectElements(
      activeField, maxLength, port, hostInput, usernameInput, passwordInput,
      hostBox, usernameBox, passwordBox, btnConnect, btnBack);

  // --- ENet Setup ---
  if (enet_initialize() != 0)
    return 1;
  atexit(enet_deinitialize);

  ENetHost *client = enet_host_create(NULL, 1, 2, 0, 0);
  ENetAddress address;
  ENetEvent event;
  ENetPeer *peer = nullptr;
  std::string warningMessage = "";
  ENetElements enetElements = {client, address, event, peer, warningMessage};

  // --- Main Loop ---
  while (!WindowShouldClose() && !menuElements.shouldQuit) {
    // Update
    mousePoint = GetMousePosition();

    // Game Screen Logic
    if (currentScreen == MENU) {
      // Check Offline Button
      MenuScreenLogic(mousePoint, menuElements, currentScreen);
    } else if (currentScreen == CONNECT) {
      ConnectScreenLogic(mousePoint, connectElements, currentScreen,
                         enetElements);
    } else if (currentScreen == GAME) {
      GameScreenLogic(mousePoint, currentScreen, enetElements);
    } else {
      // Logic to return to menu from other screens (OFFLINE, OPTIONS, WARNING)
      if (IsKeyPressed(KEY_ESCAPE)) {
        currentScreen = MENU;
      }
    }

    // Rendering
    BeginDrawing();
    ClearBackground(RAYWHITE);
    if (currentScreen == MENU) {
      DisplayMenuScreen(mousePoint, menuElements, screenOptions);
    } else if (currentScreen == GAME) {
      DisplayGameScreen();
    } else if (currentScreen == OPTIONS) {
      DisplayOptionsScreen();
    } else if (currentScreen == CONNECT) {
      DisplayConnectScreen(mousePoint, connectElements, screenOptions);
    } else if (currentScreen == WARNING) {
      DisplayWarningScreen(enetElements.warningMessage);
    }
    EndDrawing();
  }

  // Cleanup
  if (peer != nullptr) {
    enet_peer_disconnect(peer, 0);
  }
  if (client != nullptr) {
    enet_host_destroy(client);
  }

  CloseWindow();
  return 0;
}
