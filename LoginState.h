#pragma once
#include "GameState.h"
#include "SQLiteDatabase.h"
#include "ClientConnection.h"
#include "PlayingState.h"
#include "PlayerFactory.h"

class LoginState : public GameState
{
public:
    enum class LoginStep {
        USERNAME,
        PASSWORD
    };

    LoginState() : step(LoginStep::USERNAME) {}

    void OnEnter(ClientConnection* client) override {
        client->QueueMessage("\r\n=== LOGIN ===\r\n");
        client->QueueMessage("Enter your username:\r\n");
    }

    void HandleInput(ClientConnection* client, std::vector<std::string> p) override {
        if (p.empty()) return;

        GameEngine* engine = client->GetEngine();

        switch (step) {
        case LoginStep::USERNAME:
            tempUsername = p[0];

            if (!engine->gameContext.db->PlayerExists(tempUsername)) {
                client->QueueMessage("User not found.\r\n");
                client->QueueMessage("Enter your username:\r\n");
                return;
            }

            step = LoginStep::PASSWORD;
            client->QueueMessage("Enter your password:\r\n");
            break;

        case LoginStep::PASSWORD:
            if (!engine->gameContext.db->VerifyPassword(tempUsername, p[0])) {
                client->QueueMessage("Incorrect password.\r\n");
                client->QueueMessage("Enter your username:\r\n");
                step = LoginStep::USERNAME;
                tempUsername.clear();
                return;
            }

            // Password verified, load the player
            {
                EntityID playerEntity = engine->LoadPlayer(client, tempUsername);

                if (playerEntity != -1) {
                    client->playerEntityID = playerEntity;
                    client->QueueMessage("Login Successful! Entering world...\r\n");
                    client->GetEngine()->gameContext.registry->AddComponent<PlayerLoginComponent>(client->playerEntityID);
                    client->PushState(new PlayingState(engine->GetContext()));
                }
                else {
                    client->QueueMessage("Login Failed (DB error).\r\n");
                    client->QueueMessage("Enter your username:\r\n");
                    step = LoginStep::USERNAME;
                    tempUsername.clear();
                }
            }
            break;
        }
    }

private:
    LoginStep step;
    std::string tempUsername;
};
