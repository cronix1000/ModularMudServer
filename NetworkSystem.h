#pragma once
#include "Registry.h"
#include "EventBus.h"
#include "ClientComponent.h"
#include "PositionComponent.h"
#include "Room.h"
#include "World.h"
#include "GameContext.h"
#include "TextHelperFunctions.h"
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

class NetworkSystem {
	GameContext& ctx;
public:
	NetworkSystem(GameContext& gc) : ctx(gc){};
	void SetupListeners();
	void FlushQueues();
	
private:
	void SendToWebClient(ClientConnection* client, const GameMessage& msg);
	void SendToTerminalClient(ClientConnection* client, const GameMessage& msg, bool hasSideBar);
	std::string BuildJSONEnvelope(const GameMessage& msg);
	std::string BuildGMCPSession(const std::string& module, const json& data);
};
