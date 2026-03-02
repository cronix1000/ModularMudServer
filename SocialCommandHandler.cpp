#include "SocialCommandHandler.h"
#include "CommandRegistry.h"
#include "ClientConnection.h"
#include "GameContext.h"
#include "Registry.h"
#include "PositionComponent.h"
#include "NameComponent.h"

std::string SocialCommandHandler::BuildMessage(const std::vector<std::string>& params) {
	if (params.empty()) return "";
	
	std::string message = "";
	for (size_t i = 0; i < params.size(); ++i) {
		message += params[i];
		if (i < params.size() - 1) message += " ";
	}
	return message;
}

void SocialCommandHandler::RegisterAll(CommandRegistry& registry) {
	// Social commands
	registry.RegisterWithAliases("say", HandleSay, {"'"}, PermissionLevel::Player);
	registry.RegisterWithAliases("tell", HandleTell, {"whisper", "msg"}, PermissionLevel::Player);
	registry.RegisterWithAliases("shout", HandleShout, {"yell"}, PermissionLevel::Player);
	registry.RegisterWithAliases("emote", HandleEmote, {":"}, PermissionLevel::Player);
	registry.RegisterWithAliases("who", HandleWho, {"list"}, PermissionLevel::Player);
}

CommandResult SocialCommandHandler::HandleSay(ClientConnection* client,
											  const std::vector<std::string>& params,
											  GameContext& ctx) {
	if (params.empty()) {
		return CommandResult::Failure("Say what?");
	}

	std::string message = BuildMessage(params);
	EntityID playerID = client->playerEntityID;
	
	// Get player name
	auto* nameComp = ctx.registry->GetComponent<NameComponent>(playerID);
	if (!nameComp) {
		return CommandResult::Failure("You have no name!");
	}
	
	// Get player's room
	auto* posComp = ctx.registry->GetComponent<PositionComponent>(playerID);
	if (!posComp) {
		return CommandResult::Failure("You are nowhere!");
	}
	
	std::string fullMessage = nameComp->displayName + " says: \"" + message + "\"";
	
	// Broadcast to room (TODO: Implement proper broadcasting)
	client->QueueMessage("You say: \"" + message + "\"\r\n");
	
	return CommandResult::Success();
}

CommandResult SocialCommandHandler::HandleTell(ClientConnection* client,
											   const std::vector<std::string>& params,
											   GameContext& ctx) {
	if (params.size() < 2) {
		return CommandResult::Failure("Tell who what?");
	}

	std::string targetName = params[0];
	std::string message = BuildMessage(std::vector<std::string>(params.begin() + 1, params.end()));
	
	// TODO: Implement player lookup and messaging
	return CommandResult::Failure("Tell command not yet fully implemented.");
}

CommandResult SocialCommandHandler::HandleShout(ClientConnection* client,
												const std::vector<std::string>& params,
												GameContext& ctx) {
	if (params.empty()) {
		return CommandResult::Failure("Shout what?");
	}

	std::string message = BuildMessage(params);
	
	// TODO: Implement zone-wide broadcast
	client->QueueMessage("You shout: \"" + message + "\"\r\n");
	
	return CommandResult::Success();
}

CommandResult SocialCommandHandler::HandleEmote(ClientConnection* client,
												const std::vector<std::string>& params,
												GameContext& ctx) {
	if (params.empty()) {
		return CommandResult::Failure("Emote what?");
	}

	std::string action = BuildMessage(params);
	EntityID playerID = client->playerEntityID;
	
	auto* nameComp = ctx.registry->GetComponent<NameComponent>(playerID);
	if (!nameComp) {
		return CommandResult::Failure("You have no name!");
	}
	
	// TODO: Broadcast emote to room
	client->QueueMessage(nameComp->displayName + " " + action + "\r\n");
	
	return CommandResult::Success();
}

CommandResult SocialCommandHandler::HandleWho(ClientConnection* client,
											  const std::vector<std::string>& params,
											  GameContext& ctx) {
	// TODO: Count online players
	client->QueueMessage("Players online: [Not yet implemented]\r\n");
	return CommandResult::Success();
}
