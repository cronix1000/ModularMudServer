#pragma once
#include "GameState.h"
#include "ClientConnection.h"  
#include <string>
#include "PlayingState.h"
#include "ClassComponent.h"
#include "LoginState.h"
#include "SQLiteDatabase.h"
#include "GameEngine.h"

class CharCreationState : public GameState {
public:
    enum class CreationStep {
        USERNAME,
        PASSWORD,
        CONFIRM_PASSWORD
    };

    CharCreationState() : step(CreationStep::USERNAME) {}
    
    ~CharCreationState() {}
    
    void OnEnter(ClientConnection* client) override {
        client->QueueMessage("\r\n=== CHARACTER CREATION ===\r\n");
        client->QueueMessage("Enter your desired username:\r\n");
    }

    void HandleInput(ClientConnection* client, std::vector<std::string> p) override {
        if (p.empty()) return;
        
        GameEngine* engine = client->GetEngine();
        std::string input = p[0];

        switch (step) {
        case CreationStep::USERNAME:
            // Check if username already exists
            if (engine->gameContext.db->PlayerExists(input)) {
                client->QueueMessage("That username is already taken.\r\n");
                client->QueueMessage("Enter your desired username:\r\n");
                return;
            }
            
            // Check username length
            if (input.length() < 3 || input.length() > 20) {
                client->QueueMessage("Username must be between 3 and 20 characters.\r\n");
                client->QueueMessage("Enter your desired username:\r\n");
                return;
            }
            
            tempPlayer = input;
            step = CreationStep::PASSWORD;
            client->QueueMessage("Choose a password (min 4 characters):\r\n");
            break;

        case CreationStep::PASSWORD:
            // Check password length
            if (input.length() < 4) {
                client->QueueMessage("Password must be at least 4 characters.\r\n");
                client->QueueMessage("Choose a password (min 4 characters):\r\n");
                return;
            }
            
            password = input;
            step = CreationStep::CONFIRM_PASSWORD;
            client->QueueMessage("Confirm your password:\r\n");
            break;

        case CreationStep::CONFIRM_PASSWORD:
            if (input != password) {
                client->QueueMessage("Passwords do not match!\r\n");
                step = CreationStep::PASSWORD;
                client->QueueMessage("Choose a password (min 4 characters):\r\n");
                return;
            }
            
            // Create the player
            {
                PlayerData playerData;
                playerData.name = tempPlayer;
                // Set default starting position
                playerData.room_id = 1;  // Default starting room
                playerData.x = 5;
                playerData.y = 5;
                playerData.region = "default";
                
                EntityID playerEntity = engine->CreatePlayer(client, tempPlayer, password, playerData);
                
                if (playerEntity != -1) {
                    client->QueueMessage("\r\nCharacter created successfully!\r\n");
                    client->QueueMessage("Please login with your new credentials.\r\n");
                    client->PushState(new LoginState());
                } else {
                    client->QueueMessage("Error creating character. Please try again.\r\n");
                    step = CreationStep::USERNAME;
                    client->QueueMessage("Enter your desired username:\r\n");
                }
            }
            break;
        }
    }

private:
    CreationStep step;
    std::string tempPlayer;
    std::string password;
};
