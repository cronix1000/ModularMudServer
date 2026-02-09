#pragma once
#include "GameState.h"
#include "SQLiteDatabase.h"
#include "ClientConnection.h"
#include "PlayingState.h"

class LoginState : public GameState
{
public:
	void OnEnter(ClientConnection* client) override {
		client->QueueMessage("Welcome! Please enter your username:\r\n");
	}
	void HandleInput(ClientConnection* client, std::vector<std::string> p) override {


		GameEngine* engine = client->GetEngine();
		EntityID playerEntity = engine->LoadPlayer(client, p[0]);
		
		if (playerEntity != -1) {
			// Success!
			client->playerEntityID=playerEntity; 
			client->QueueMessage("Login Successful! Entering world...\r\n");
			client->GetEngine()->gameContext.registry->AddComponent<PlayerLoginComponent>(client->playerEntityID);
			client->PushState(new PlayingState(engine->GetContext()));
		}
		else {
			client->QueueMessage("Login Failed (User not found or DB error).\r\nTry Again:\r\n");
		}
	}
private:

};
