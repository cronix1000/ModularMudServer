#pragma once

#include "CommandTrie.h"

using EntityID = int;

class ClientConnection;
struct GameContext;
class CommandRegistry;

class StatsCommandHandler {
public:
	static void RegisterAll(CommandRegistry& registry);

private:
	static CommandResult HandleStats(ClientConnection* client,
	                                 const std::vector<std::string>& params,
	                                 GameContext& ctx);
};
