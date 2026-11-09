#pragma once
#include "GameState.h"

using EntityID = int;
class DialogueState : public GameState
{
	DialogueNode* currentNode;
	EntityID npcId;
public:
	DialogueState();
	~DialogueState();

	void HandleInput(ClientConnection* client, std::vector<std::string> p) override {
	
		if (input >= KEY_1 && input <= KEY_4) {
			int choice = input - KEY_1;
			if (choice < currentNode->options.size()) {
				ExecuteOption(currentNode->options[choice]);
			}
		}
	}

private:

};
