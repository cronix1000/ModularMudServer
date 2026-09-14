#pragma once
#include "Registry.h"
#include "EventBus.h"
#include "ClientComponent.h"
#include "PositionComponent.h"
#include "World.h"
#include "GameContext.h"
#include "TextHelperFunctions.h"
#include "CommandTrie.h"
#include "GMCPModules.h"
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
	void SendToWebClient(ClientConnection* client, ClientComponent* clientComp, const GameMessage& msg);
	void SendToTerminalClient(ClientConnection* client, ClientComponent* clientComp, const GameMessage& msg);

	std::string BuildWebEnvelope(const std::string& module,
	                             const std::string& consoleText,
	                             const std::string& jsonDataStr);
	std::string BuildGMCPFrame(const std::string& module, const std::string& jsonDataStr);

	json BuildCommandListJson(PermissionLevel playerPerm);
};
