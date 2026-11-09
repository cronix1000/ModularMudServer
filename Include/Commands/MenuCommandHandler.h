#pragma once


#include "CommandTrie.h"

// Type definitions
using EntityID = int;

// Forward declarations
class ClientConnection;
struct GameContext;
class CommandRegistry;

class MenuCommandHandler {
public:
	static void RegisterAll(CommandRegistry& registry);

private:
	


	// Helper function to find target by name
	static EntityID FindTarget(GameContext& ctx, EntityID playerID, const std::string& targetName);

	// Helper to reconstruct name from parameters
	static std::string BuildTargetName(const std::vector<std::string>& params);
};
