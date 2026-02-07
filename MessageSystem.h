#pragma once
#include <string>
#include "GameContext.h"

class GameContext;
class Registry;

class MessageSystem
{
public:
	GameContext& ctx;
	MessageSystem(GameContext& g) : ctx(g) {};
	~MessageSystem();
	void SanitizeMessage(std::string& message);
	void SubscribeToEvents();
	void ToPlayer(int entityID, std::string msg);

	// Send to everyone in the room (Web Novel 'System' broadcasts)
	void ToRoom(int roomID, std::string msg, int excludeID = -1);
	void ToGlobal(std::string msg);

private:

};