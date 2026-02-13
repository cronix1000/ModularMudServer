#pragma once
#include "GameState.h"
#include "ClientConnection.h"  
#include <string>
#include "PlayingState.h"
#include "ClassComponent.h"
#include "LoginState.h"
class CharCreationState : public GameState {
    int step = 0;
    std::string tempPlayer;
    std::string password;
public:
    CharCreationState() {

    }
    ~CharCreationState() {

    }
    void OnEnter(ClientConnection* client) override {
        client->QueueMessage("ENTERING CHARACTER CREATION...\r\nName your hero: ");
    }

    void HandleInput(ClientConnection* client, std::vector<std::string> p) override {

        if (step == 0) {
            // Logic: Save Name
            tempPlayer = p[0];

            client->QueueMessage("Great name! Now, Choose a password: ");
            std::cin >> password;
            step++;
        }
        else if (step == 1) {
            client->QueueMessage("Character Saved! Returning to Main Menu...\r\n");
            PlayerData playerData;
            client->GetEngine()->CreatePlayer(client, tempPlayer, password,playerData);


            client->PushState(new LoginState());

        }
        else {
            client->QueueMessage("Invalid class. Type Warrior or Mage: ");
        }
    }
};