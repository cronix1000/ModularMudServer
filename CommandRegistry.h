#pragma once

#include "CommandTrie.h"
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

// Third-party includes
#include <sol/sol.hpp>
#include <nlohmann/json.hpp>

// Type definitions
using EntityID = int;
using json = nlohmann::json;

// Forward declarations
class ClientConnection;
struct GameContext;
struct CommandNode;

using json = nlohmann::json;

class CommandRegistry {
public:
	CommandRegistry(GameContext& ctx, sol::state& lua);
	~CommandRegistry();
	
	// C++ command registration
	void Register(const std::string& commandPath,
				  CommandHandler handler,
				  PermissionLevel minPerm = PermissionLevel::Guest);
	
	// Register with aliases
	void RegisterWithAliases(const std::string& primaryPath,
							 CommandHandler handler,
							 const std::vector<std::string>& aliases,
							 PermissionLevel minPerm = PermissionLevel::Guest);
	
	// Lua command registration (exposed to Lua)
	void RegisterLua(const std::string& commandPath,
					 sol::protected_function luaFunc,
					 PermissionLevel minPerm);
	
	// Execute with chaining support
	void Execute(ClientConnection* client, const std::string& input);
	
	// Execute a single command (returns whether to continue chain)
	bool ExecuteSingle(ClientConnection* client, const std::vector<std::string>& words);
	
	// Send command list to client as JSON
	void SendCommandList(ClientConnection* client) const;
	
	// Get command list as JSON for a specific permission level
	nlohmann::json GetCommandListJson(PermissionLevel level) const;
	
	// Check permission
	bool HasPermission(EntityID playerId, PermissionLevel required) const;
	
	// Get player permission level
	PermissionLevel GetPlayerPermission(EntityID playerId) const;
	
	// Setup Lua bindings
	void SetupLuaBindings();
	
private:
	GameContext& ctx_;
	sol::state& lua_;
	CommandTrie trie_;
	
	void ExecuteLuaCommand(ClientConnection* client,
						   CommandNode* node,
						   const std::vector<std::string>& params);
	
	// Lua-exposed registration function
	void LuaRegisterCommand(const std::string& path,
							sol::protected_function handler,
							uint8_t minPerm);
	
	// Build JSON for command list
	json BuildCommandJson(CommandNode* node) const;
};
