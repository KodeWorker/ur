#include "client/screen/connect.hpp"
#include "client/fbs.hpp"

namespace ur::screen {

Connect::Connect(ScreenSettings screenSettings)
    : hostInput("localhost"), usernameInput(""), passwordInput(""),
      hostBox{(float)screenSettings.width / 2 - 100, 120, 200, 30},
      usernameBox{(float)screenSettings.width / 2 - 100, 170, 200, 30},
      passwordBox{(float)screenSettings.width / 2 - 100, 220, 200, 30},
      btnConnect{(float)screenSettings.width / 2 - 75, 270, 150, 40},
      btnBack{(float)screenSettings.width / 2 - 75, 320, 150, 40} {}

void Connect::Logic(const Vector2 &mousePoint, ENetElements &enetElements,
                    GameScreen &currentScreen) {
  // Handle text input for connection screen
  if (CheckCollisionPointRec(mousePoint, hostBox) &&
      IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    activeField = 0;
  } else if (CheckCollisionPointRec(mousePoint, usernameBox) &&
             IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    activeField = 1;
  } else if (CheckCollisionPointRec(mousePoint, passwordBox) &&
             IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    activeField = 2;
  } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
             !CheckCollisionPointRec(mousePoint, hostBox) &&
             !CheckCollisionPointRec(mousePoint, usernameBox) &&
             !CheckCollisionPointRec(mousePoint, passwordBox)) {
    activeField = -1;
  }

  // Handle keyboard input for active field
  if (activeField >= 0) {
    std::string &currentInput = (activeField == 0)   ? hostInput
                                : (activeField == 1) ? usernameInput
                                                     : passwordInput;
    int currentLength = currentInput.length();

    // Get character pressed
    int key = GetCharPressed();
    while (key > 0) {
      if (key >= 32 && key <= 125 && currentLength < maxLength) {
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
  if (CheckCollisionPointRec(mousePoint, btnConnect)) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      // Setup server address
      enet_address_set_host(&enetElements.address, hostInput.c_str());
      enetElements.address.port = port;
      // Connect to server
      enetElements.peer =
          enet_host_connect(enetElements.client, &enetElements.address, 2, 0);
      // Timeout for connection attempt
      if (enet_host_service(enetElements.client, &enetElements.event, 5000) >
              0 &&
          enetElements.event.type == ENET_EVENT_TYPE_CONNECT) {
        ur::fbs::SendClient(enetElements.peer, usernameInput, passwordInput);
        TraceLog(LOG_INFO, "Sent Client FlatBuffer packet for user: %s",
                 usernameInput.c_str());
        enet_peer_ping_interval(enetElements.peer,
                                5000); // heartbeat every 5 seconds
        currentScreen = GAME;
      } else {
        // Either 5 seconds passed or we got a DISCONNECT event
        enet_peer_reset(enetElements.peer);
        enetElements.peer = nullptr;
        enetElements.warningMessage = "Connection failed - timeout";
        currentScreen = WARNING;
        TraceLog(LOG_WARNING, "Connection failed to %s:%d", hostInput.c_str(),
                 port);
      }
    }
  }

  // Back button
  if (CheckCollisionPointRec(mousePoint, btnBack)) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      currentScreen = MENU;
    }
  }
}

void Connect::Display(const Vector2 &mousePoint,
                      const ScreenSettings &screenSettings) {
  DrawText("CONNECT TO SERVER",
           screenSettings.width / 2 - MeasureText("CONNECT TO SERVER", 40) / 2,
           40, 40, DARKBLUE);
  // Draw Host input box
  DrawRectangleRec(hostBox, activeField == 0 ? LIGHTGRAY : RAYWHITE);
  DrawRectangleLinesEx(hostBox, 2, activeField == 0 ? BLUE : DARKGRAY);
  DrawText("Host:", hostBox.x - 55, hostBox.y + 10, 20, DARKGRAY);
  DrawText(hostInput.c_str(), hostBox.x + 5, hostBox.y + 10, 20, BLACK);

  // Draw Username input box
  DrawRectangleRec(usernameBox, activeField == 1 ? LIGHTGRAY : RAYWHITE);
  DrawRectangleLinesEx(usernameBox, 2, activeField == 1 ? BLUE : DARKGRAY);
  DrawText("Username:", usernameBox.x - 110, usernameBox.y + 10, 20, DARKGRAY);
  DrawText(usernameInput.c_str(), usernameBox.x + 5, usernameBox.y + 10, 20,
           BLACK);

  // Draw Password input box
  DrawRectangleRec(passwordBox, activeField == 2 ? LIGHTGRAY : RAYWHITE);
  DrawRectangleLinesEx(passwordBox, 2, activeField == 2 ? BLUE : DARKGRAY);
  DrawText("Password:", passwordBox.x - 110, passwordBox.y + 10, 20, DARKGRAY);

  // Display password as asterisks
  std::string maskedPassword(passwordInput.length(), '*');
  DrawText(maskedPassword.c_str(), passwordBox.x + 5, passwordBox.y + 10, 20,
           BLACK);

  // Draw Connect button
  DrawRectangleRec(btnConnect, CheckCollisionPointRec(mousePoint, btnConnect)
                                   ? DARKGREEN
                                   : GREEN);
  DrawText("CONNECT", btnConnect.x + 30, btnConnect.y + 15, 20, WHITE);

  // Draw Back button
  DrawRectangleRec(
      btnBack, CheckCollisionPointRec(mousePoint, btnBack) ? LIGHTGRAY : GRAY);
  DrawText("BACK", btnBack.x + 50, btnBack.y + 15, 20, BLACK);
}
} // namespace ur::screen
