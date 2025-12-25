#include <iostream>
#include <string>
#include <raylib.h>
#include <enet/enet.h>
#include <flatbuffers/flatbuffers.h>
#include "client_generated.h"
#include "player_generated.h"

using namespace ur::fbs;

// Define the different screens/states of our game
typedef enum GameScreen { MENU, CONNECT, GAME, OPTIONS } GameScreen;

void SendMessage(ENetPeer* peer, const std::string& message) {
    ENetPacket* packet = enet_packet_create(message.c_str(), message.length() + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
    TraceLog(LOG_INFO, "Sent packet: %s", message.c_str());
}

void SendClient(ENetPeer* peer, const std::string& username, const std::string& password) {
    flatbuffers::FlatBufferBuilder builder;

    auto usernameOffset = builder.CreateString(username);
    auto passwordOffset = builder.CreateString(password);

    auto client = CreateClient(builder, 0, usernameOffset, passwordOffset); // id = 0 for new client
    builder.Finish(client);

    ENetPacket* packet = enet_packet_create(builder.GetBufferPointer(), builder.GetSize(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
    TraceLog(LOG_INFO, "Sent Client FlatBuffer packet for user: %s", username.c_str());
}

int main() {
    // --- Raylib Setup ---    
    const int screenWidth = 800;
    const int screenHeight = 450;
    InitWindow(screenWidth, screenHeight, "Game Client");
    SetExitKey(KEY_NULL); // Disable default ESC to close window behavior
    SetTargetFPS(60);

    GameScreen currentScreen = MENU;

    // Define button rectangles
    Rectangle btnOffline = { screenWidth/2 - 100, 150, 200, 50 };
    Rectangle btnOnline = { screenWidth/2 - 100, 220, 200, 50 };
    Rectangle btnOptions = { screenWidth/2 - 100, 290, 200, 50 };
    Rectangle btnQuit = { screenWidth/2 - 100, 360, 200, 50 };

    Vector2 mousePoint = { 0.0f, 0.0f };
    bool shouldQuit = false;

    // Connection input fields
    std::string hostInput = "127.0.0.1";
    std::string usernameInput = "";
    std::string passwordInput = "";
    int activeField = -1; // -1: none, 0: host, 1: username, 2: password
    const int port = 9487;

    Rectangle hostBox = { screenWidth/2 - 150, 120, 300, 40 };
    Rectangle usernameBox = { screenWidth/2 - 150, 180, 300, 40 };
    Rectangle passwordBox = { screenWidth/2 - 150, 240, 300, 40 };
    Rectangle btnConnect = { screenWidth/2 - 100, 300, 200, 50 };
    Rectangle btnBack = { screenWidth/2 - 100, 370, 200, 50 };
    const int maxLength = 32;

    // --- ENet Setup ---
    if (enet_initialize() != 0) return 1;
    atexit(enet_deinitialize);

    ENetHost* client = enet_host_create(NULL, 1, 2, 0, 0);
    ENetAddress address;
    ENetEvent event;
    ENetPeer* peer = nullptr;
    
    bool isConnected = false;

    // --- Main Loop ---
    while (!WindowShouldClose() && !shouldQuit) {
        // Update
        mousePoint = GetMousePosition();

        // Game Screen Logic
        if (currentScreen == MENU) {
            // Check Offline Button
            if (CheckCollisionPointRec(mousePoint, btnOffline)) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) currentScreen = GAME;
            }
            // Check Online Button
            if (CheckCollisionPointRec(mousePoint, btnOnline)) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) currentScreen = CONNECT;
            }
            // Check Options Button
            if (CheckCollisionPointRec(mousePoint, btnOptions)) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) currentScreen = OPTIONS;
            }
            // Check Quit Button
            if (CheckCollisionPointRec(mousePoint, btnQuit)) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) shouldQuit = true;
            }
        } else if (currentScreen == CONNECT) {
            // Handle text input for connection screen
            if (CheckCollisionPointRec(mousePoint, hostBox) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                activeField = 0;
            } else if (CheckCollisionPointRec(mousePoint, usernameBox) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                activeField = 1;
            } else if (CheckCollisionPointRec(mousePoint, passwordBox) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                activeField = 2;
            } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && 
                       !CheckCollisionPointRec(mousePoint, hostBox) &&
                       !CheckCollisionPointRec(mousePoint, usernameBox) &&
                       !CheckCollisionPointRec(mousePoint, passwordBox)) {
                activeField = -1;
            }

            // Handle keyboard input for active field
            if (activeField >= 0) {
                std::string& currentInput = (activeField == 0) ? hostInput : (activeField == 1) ? usernameInput : passwordInput;
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
                    enet_address_set_host(&address, hostInput.c_str());
                    address.port = port;
                    // Connect to server
                    peer = enet_host_connect(client, &address, 2, 0);
                    // Timeout for connection attempt
                    if (enet_host_service(client, &event, 5000) > 0 && 
                        event.type == ENET_EVENT_TYPE_CONNECT) {
                            SendClient(peer, usernameInput, passwordInput);
                            currentScreen = GAME;
                            isConnected = true;
                    } else {
                        // Either 5 seconds passed or we got a DISCONNECT event
                        enet_peer_reset(peer);
                        TraceLog(LOG_WARNING, "Connection failed to %s:%d", hostInput, port);
                    }
                }
            }
            
            // Back button
            if (CheckCollisionPointRec(mousePoint, btnBack)) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    currentScreen = MENU;
                }
            }
            
        } else if (currentScreen == GAME) {

            // 1. Networking (Non-blocking poll)
            while (enet_host_service(client, &event, 0) > 0) {
                switch (event.type) {
                    case ENET_EVENT_TYPE_CONNECT:
                        isConnected = true;
                        TraceLog(LOG_INFO, "Successfully connected to server");
                        break;
                    case ENET_EVENT_TYPE_RECEIVE: {
                        auto client = GetClient(event.packet->data);                        
                        // Verify the flatbuffer
                        flatbuffers::Verifier verifier(event.packet->data, event.packet->dataLength);
                        if (!client->Verify(verifier)) {
                            std::cerr << "Invalid flatbuffer received" << std::endl;
                        }
                        else{
                            if(client->status() == Status::Status_AUTHENTICATED_SUCCESS) {
                                TraceLog(LOG_INFO, "Authenticated by server as user: %s with ID: %d", 
                                        client->username()->c_str(), client->id());
                            } else if(client->status() == Status::Status_AUTHENTICATED_FAIL) {
                                enet_peer_disconnect(peer, 0);
                                isConnected = false;
                                peer = nullptr;
                                currentScreen = CONNECT;
                                TraceLog(LOG_WARNING, "Authentication failed for user: %s", client->username()->c_str());
                            } else {
                                TraceLog(LOG_INFO, "Received message from server for user: %s", client->username()->c_str());
                            }
                        }
                        enet_packet_destroy(event.packet);
                        break;
                    }
                    case ENET_EVENT_TYPE_DISCONNECT:
                        isConnected = false;
                        peer = nullptr;
                        currentScreen = MENU;
                        TraceLog(LOG_WARNING, "Disconnected from server");
                        break;
                }
            }

            // 2. Input
            if (IsKeyPressed(KEY_SPACE)) {
                if (isConnected && peer != nullptr) {
                    SendMessage(peer, "Hello Server!");
                }
            } else if (IsKeyPressed(KEY_ESCAPE)) {
                if (isConnected && peer != nullptr) {
                    enet_peer_disconnect(peer, 0);
                    enet_peer_reset(peer);
                    peer = nullptr;
                    TraceLog(LOG_INFO, "Disconnecting from server...");
                }
                isConnected = false;
                currentScreen = MENU;
            }

        } else {
            // Logic to return to menu from other screens
            if (IsKeyPressed(KEY_ESCAPE)) currentScreen = MENU;
        }

        // Rendering
        BeginDrawing();
            ClearBackground(RAYWHITE);
            if (currentScreen == MENU) {
                DrawText("MAIN MENU", screenWidth/2 - MeasureText("MAIN MENU", 40)/2, 60, 40, DARKGRAY);

                // Draw Offline Button
                DrawRectangleRec(btnOffline, CheckCollisionPointRec(mousePoint, btnOffline) ? LIGHTGRAY : GRAY);
                DrawText("OFFLINE", btnOffline.x + 60, btnOffline.y + 15, 20, BLACK);
                
                // Draw Online Button
                DrawRectangleRec(btnOnline, CheckCollisionPointRec(mousePoint, btnOnline) ? LIGHTGRAY : GRAY);
                DrawText("ONLINE", btnOnline.x + 60, btnOnline.y + 15, 20, BLACK);

                // Draw Options Button
                DrawRectangleRec(btnOptions, CheckCollisionPointRec(mousePoint, btnOptions) ? LIGHTGRAY : GRAY);
                DrawText("OPTIONS", btnOptions.x + 50, btnOptions.y + 15, 20, BLACK);

                // Draw Quit Button
                DrawRectangleRec(btnQuit, CheckCollisionPointRec(mousePoint, btnQuit) ? RED : MAROON);
                DrawText("QUIT", btnQuit.x + 70, btnQuit.y + 15, 20, WHITE);

            } else if (currentScreen == GAME) {
                DrawText("GAME SCREEN", 20, 20, 40, MAROON);
                DrawText("Press SPACE to send a \"Hello Server!\" message", 20, 80, 20, DARKGRAY);
                DrawText("Press ESC to return to Menu", 20, 110, 20, DARKGRAY);
            } else if (currentScreen == OPTIONS) {
                DrawText("OPTIONS SCREEN", 20, 20, 40, DARKBLUE);
                DrawText("Press ESC to return to Menu", 20, 80, 20, DARKGRAY);
            } else if (currentScreen == CONNECT) {
                DrawText("CONNECT TO SERVER", screenWidth/2 - MeasureText("CONNECT TO SERVER", 40)/2, 40, 40, DARKBLUE);
                
                // Draw Host input box
                DrawRectangleRec(hostBox, activeField == 0 ? LIGHTGRAY : RAYWHITE);
                DrawRectangleLinesEx(hostBox, 2, activeField == 0 ? BLUE : DARKGRAY);
                DrawText("Host:", hostBox.x - 55, hostBox.y + 10, 20, DARKGRAY);
                DrawText(hostInput.c_str(), hostBox.x + 5, hostBox.y + 10, 20, BLACK);
                
                // Draw Username input box
                DrawRectangleRec(usernameBox, activeField == 1 ? LIGHTGRAY : RAYWHITE);
                DrawRectangleLinesEx(usernameBox, 2, activeField == 1 ? BLUE : DARKGRAY);
                DrawText("Username:", usernameBox.x - 110, usernameBox.y + 10, 20, DARKGRAY);
                DrawText(usernameInput.c_str(), usernameBox.x + 5, usernameBox.y + 10, 20, BLACK);
                
                // Draw Password input box
                DrawRectangleRec(passwordBox, activeField == 2 ? LIGHTGRAY : RAYWHITE);
                DrawRectangleLinesEx(passwordBox, 2, activeField == 2 ? BLUE : DARKGRAY);
                DrawText("Password:", passwordBox.x - 110, passwordBox.y + 10, 20, DARKGRAY);
                
                // Display password as asterisks
                std::string maskedPassword(passwordInput.length(), '*');
                DrawText(maskedPassword.c_str(), passwordBox.x + 5, passwordBox.y + 10, 20, BLACK);
                
                // Draw Connect button
                DrawRectangleRec(btnConnect, CheckCollisionPointRec(mousePoint, btnConnect) ? DARKGREEN : GREEN);
                DrawText("CONNECT", btnConnect.x + 50, btnConnect.y + 15, 20, WHITE);
                
                // Draw Back button
                DrawRectangleRec(btnBack, CheckCollisionPointRec(mousePoint, btnBack) ? LIGHTGRAY : GRAY);
                DrawText("BACK", btnBack.x + 70, btnBack.y + 15, 20, BLACK);
            }
        EndDrawing();
    }
    
    // Cleanup
    if (isConnected && peer != nullptr) {
        enet_peer_disconnect(peer, 0);
    }
    if (client != nullptr) {
        enet_host_destroy(client);
    }
    
    CloseWindow();
    return 0;
}