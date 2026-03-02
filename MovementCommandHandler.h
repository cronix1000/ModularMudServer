#pragma once

#include "CommandTrie.h"

// Type definitions
using EntityID = int;

// Forward declarations
class ClientConnection;
struct GameContext;
class CommandRegistry;

class MovementCommandHandler {
public:
	static void RegisterAll(CommandRegistry& registry);
	
private:
	static CommandResult HandleMove(ClientConnection* client,
									const std::vector<std::string>& params,
									GameContext& ctx);
	
	static CommandResult HandleNorth(ClientConnection* client,
									 const std::vector<std::string>& params,
									 GameContext& ctx);
	
	static CommandResult HandleSouth(ClientConnection* client,
									 const std::vector<std::string>& params,
									 GameContext& ctx);
	
	static CommandResult HandleEast(ClientConnection* client,
									const std::vector<std::string>& params,
									GameContext& ctx);
	
	static CommandResult HandleWest(ClientConnection* client,
									const std::vector<std::string>& params,
									GameContext& ctx);
	
	static CommandResult HandleUp(ClientConnection* client,
								  const std::vector<std::string>& params,
								  GameContext& ctx);
	
	static CommandResult HandleDown(ClientConnection* client,
									const std::vector<std::string>& params,
									GameContext& ctx);
	
	static CommandResult HandleClimb(ClientConnection* client,
									 const std::vector<std::string>& params,
									 GameContext& ctx);
};
