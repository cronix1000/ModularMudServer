#include "CommandRegistry.h"
#include "ClientConnection.h"
#include "GameContext.h"
#include "Registry.h"
#include "PermissionComponent.h"
#include "CommandChain.h"
#include <algorithm>

CommandRegistry::CommandRegistry(GameContext& ctx, sol::state& lua)
	: ctx_(ctx), lua_(lua) {
	SetupLuaBindings();
}

CommandRegistry::~CommandRegistry() = default;

void CommandRegistry::Register(const std::string& commandPath,
							   CommandHandler handler,
							   PermissionLevel minPerm) {
	// Split path into words
	std::vector<std::string> path;
	std::stringstream ss(commandPath);
	std::string word;
	
	while (ss >> word) {
		// Convert to lowercase
		std::transform(word.begin(), word.end(), word.begin(), ::tolower);
		path.push_back(word);
	}
	
	if (!path.empty()) {
		trie_.Insert(path, handler, minPerm);
	}
}

void CommandRegistry::RegisterWithAliases(const std::string& primaryPath,
										  CommandHandler handler,
										  const std::vector<std::string>& aliases,
										  PermissionLevel minPerm) {
	// Register primary path
	Register(primaryPath, handler, minPerm);
	
	// Register aliases (pointing to same handler)
	for (const auto& alias : aliases) {
		Register(alias, handler, minPerm);
	}
}

void CommandRegistry::RegisterLua(const std::string& commandPath,
								  sol::protected_function luaFunc,
								  PermissionLevel minPerm) {
	// Split path
	std::vector<std::string> path;
	std::stringstream ss(commandPath);
	std::string word;
	
	while (ss >> word) {
		std::transform(word.begin(), word.end(), word.begin(), ::tolower);
		path.push_back(word);
	}
	
	if (path.empty()) return;
	
	// Create wrapper that calls Lua
	CommandHandler wrapper = [this, luaFunc](ClientConnection* client,
										   const std::vector<std::string>& params,
										   GameContext& ctx) -> CommandResult {
		// Convert params to Lua table
		sol::table luaParams = lua_.create_table();
		for (size_t i = 0; i < params.size(); ++i) {
			luaParams[i + 1] = params[i];
		}
		
		// Call Lua function
		sol::protected_function_result result = luaFunc(client, luaParams);
		
		if (!result.valid()) {
			sol::error err = result;
			return CommandResult::Failure("Command error: " + std::string(err.what()));
		}
		
		// Lua returns: {success = true/false, message = "..."}
		sol::table returnVal = result;
		bool success = returnVal.get_or("success", false);
		std::string message = returnVal.get_or("message", std::string(""));
		
		return CommandResult(success, message);
	};
	
	trie_.Insert(path, wrapper, minPerm);
}

void CommandRegistry::Execute(ClientConnection* client, const std::string& input) {
	if (!client) return;
	
	// Parse chain
	std::vector<std::vector<std::string>> commands = CommandChain::Parse(input);
	
	// Execute each command in sequence
	for (const auto& cmdWords : commands) {
		if (cmdWords.empty()) continue;
		
		bool shouldContinue = ExecuteSingle(client, cmdWords);
		
		if (!shouldContinue) {
			// Chain stopped
			break;
		}
	}
}

bool CommandRegistry::ExecuteSingle(ClientConnection* client, const std::vector<std::string>& words) {
	if (words.empty()) return true;
	
	// Find matching command in trie
	CommandTrie::MatchResult match = trie_.Match(words);
	
	if (!match.found) {
		client->QueueMessage("I don't understand that command.\r\n");
		return false; // Stop chain
	}
	
	// Check permission
	if (!HasPermission(client->playerEntityID, match.node->minPermission)) {
		client->QueueMessage("You don't have permission to use that command.\r\n");
		return false;
	}
	
	// Execute handler
	CommandResult result = match.node->handler(client, match.remaining, ctx_);
	
	// Send failure message if any
	if (!result.success && !result.message.empty()) {
		client->QueueMessage(result.message + "\r\n");
	}
	
	return result.success; // Return whether to continue chain
}

void CommandRegistry::SendCommandList(ClientConnection* client) const {
	if (!client) return;
	
	PermissionLevel playerPerm = GetPlayerPermission(client->playerEntityID);
	
	// Collect all commands at or below player's permission
	std::vector<CommandNode*> commands;
	trie_.CollectByPermission(commands, playerPerm);
	
	// Build JSON
	json cmdList = json::array();
	for (CommandNode* node : commands) {
		cmdList.push_back(BuildCommandJson(node));
	}
	
	json response;
	response["type"] = "command_list";
	response["commands"] = cmdList;
	
	client->QueueMessage(response.dump() + "\n");
}

nlohmann::json CommandRegistry::GetCommandListJson(PermissionLevel level) const {
	// Collect all commands at or below player's permission
	std::vector<CommandNode*> commands;
	trie_.CollectByPermission(commands, level);
	
	// Build JSON
	nlohmann::json cmdList = nlohmann::json::array();
	for (CommandNode* node : commands) {
		cmdList.push_back(BuildCommandJson(node));
	}
	
	return cmdList;
}

bool CommandRegistry::HasPermission(EntityID playerId, PermissionLevel required) const {
	PermissionLevel playerPerm = GetPlayerPermission(playerId);
	return playerPerm >= required;
}

PermissionLevel CommandRegistry::GetPlayerPermission(EntityID playerId) const {
	auto* permComp = ctx_.registry->GetComponent<PermissionComponent>(playerId);
	if (permComp) {
		return static_cast<PermissionLevel>(permComp->level);
	}
	return PermissionLevel::Guest;
}

void CommandRegistry::SetupLuaBindings() {
	// Expose RegisterCommand to Lua
	lua_.set_function("RegisterCommand", [this](sol::table cmdDef) {
		std::string name = cmdDef.get_or("name", std::string(""));
		if (name.empty()) return;
		
		sol::protected_function handler = cmdDef.get<sol::protected_function>("handler");
		uint8_t minPerm = cmdDef.get_or("permission", 0);
		
		RegisterLua(name, handler, static_cast<PermissionLevel>(minPerm));
		
		// Handle aliases if present
		if (cmdDef["aliases"].valid()) {
			sol::table aliases = cmdDef["aliases"];
			for (size_t i = 1; i <= aliases.size(); ++i) {
				std::string alias = aliases.get<std::string>(i);
				RegisterLua(alias, handler, static_cast<PermissionLevel>(minPerm));
			}
		}
	});
}

json CommandRegistry::BuildCommandJson(CommandNode* node) const {
	json cmd;
	cmd["name"] = node->fullPath;
	cmd["description"] = ""; // Could be added to CommandNode
	cmd["permission"] = static_cast<int>(node->minPermission);
	
	if (!node->aliases.empty()) {
		cmd["aliases"] = node->aliases;
	}
	
	return cmd;
}
