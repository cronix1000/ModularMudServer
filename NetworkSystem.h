#pragma once
#include "Registry.h"
#include "EventBus.h"
#include "ClientComponent.h"
#include "PositionComponent.h"
#include "World.h"
#include "GameContext.h"
#include "TextHelperFunctions.h"
#include "CommandTrie.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

using json = nlohmann::json;

class NetworkSystem {
	GameContext& ctx;
public:
	NetworkSystem(GameContext& gc) : ctx(gc){};
	void SetupListeners();
	void FlushQueues();
	
	// Send command list to client (for autocomplete)
	void SendCommandList(EntityID playerId);
	
private:
	void SendToWebClient(ClientConnection* client, const GameMessage& msg);
	void SendToTerminalClient(ClientConnection* client, const GameMessage& msg, bool hasSideBar);
	std::string BuildJSONEnvelope(const GameMessage& msg);
	std::string BuildGMCPSession(const std::string& moduleName, const std::string& jsonDataStr);
	
	// Build command list JSON
	json BuildCommandListJson(PermissionLevel playerPerm);
};
