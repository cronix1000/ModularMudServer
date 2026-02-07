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
    std::string tempClass;
public:
    CharCreationState() {

    }
    ~CharCreationState() {

    }
    void OnEnter(ClientConnection* client) override {
        client->QueueMessage("ENTERING CHARACTER CREATION...\r\nName your hero: ");
    }

    void HandleInput(ClientConnection* client, std::vector<std::string> p) override {
        printf("[CharCreation] Step: %d | Input: '%s'\n", step);

        
        if (step == 0) {
            // Logic: Save Name
            tempPlayer = p[0];
           
            client->QueueMessage("Great name! Now, choose (Warrior/Mage): ");
            step++;
        }
        else if (step == 1) {
            // Logic: Save Class
            if (p[0] == "Warrior" || p[0] == "Mage") {
                tempClass= p[0];
                client->QueueMessage("Character Saved! Returning to Main Menu...\r\n");
                PlayerData playerData;
                 client->GetEngine()->CreatePlayer(client, tempPlayer, playerData);


                client->PushState(new LoginState());

            }
            else {
                client->QueueMessage("Invalid class. Type Warrior or Mage: ");
            }
        }
    }
};