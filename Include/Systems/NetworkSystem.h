#pragma once
#include "Registry.h"
#include "EventBus.h"
#include "MovementComponents.h"
#include "PlayerComponents.h"
#include "World.h"
#include "GameContext.h"
#include "TextHelperFunctions.h"
#include "CommandTrie.h"
#include "TelnetCodec.h"
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

	void SendCommandList(EntityID playerId);

private:
	void SendToTerminalClient(ClientConnection* client, ClientComponent* clientComp, const GameMessage& msg);

	std::string BuildGMCPFrame(const std::string& module, const std::string& jsonDataStr);

	json BuildCommandListJson(PermissionLevel playerPerm);
};
