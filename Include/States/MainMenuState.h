#pragma once

#include "GameState.h"
#include "ClientConnection.h"  
#include <string>
#include "CharCreatorState.h"
#include "LoginState.h"
class MainMenuState : public GameState {
    void OnEnter(ClientConnection* client) override {
        client->QueueMessage("ENTERING MAIN MENU\r\nType Login to go to login\r\nType Anything else to go to Char creation ");
    }

    void HandleInput(ClientConnection* client, std::vector<std::string> p) override {
        std::transform(p[0].begin(), p[0].end(), p[0].begin(), ::tolower);


        if(p[0] == "login")
        {
            client->PushState(new LoginState());
        }
        else {

            client->PushState(new CharCreationState());
        }
    }
};
