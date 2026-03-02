#include "MovementCommandHandler.h"
#include "CommandRegistry.h"
#include "ClientConnection.h"
#include "GameContext.h"
#include "Registry.h"
#include "MoveIntentComponent.h"
#include "Direction.h"
#include <map>

void MovementCommandHandler::RegisterAll(CommandRegistry& registry) {
	// Direction shortcuts
	registry.Register("north", HandleNorth, PermissionLevel::Player);
	registry.Register("south", HandleSouth, PermissionLevel::Player);
	registry.Register("east", HandleEast, PermissionLevel::Player);
	registry.Register("west", HandleWest, PermissionLevel::Player);
	registry.Register("up", HandleUp, PermissionLevel::Player);
	registry.Register("down", HandleDown, PermissionLevel::Player);
	
	// Full move command with direction parameter
	registry.Register("move", HandleMove, PermissionLevel::Player);
	registry.Register("climb", HandleClimb, PermissionLevel::Player);
}

CommandResult MovementCommandHandler::HandleMove(ClientConnection* client,
												 const std::vector<std::string>& params,
												 GameContext& ctx) {
	static const std::map<std::string, Direction> directionMap = {
		{"north", Direction::North},
		{"south", Direction::South},
		{"east", Direction::East},
		{"west", Direction::West},
		{"up", Direction::Up},
		{"down", Direction::Down}
	};

	if (params.empty()) {
		return CommandResult::Failure("Move in which direction?");
	}

	Direction direction = Direction::None;
	auto it = directionMap.find(params[0]);
	if (it != directionMap.end()) {
		direction = it->second;
	}

	if (direction != Direction::None) {
		ctx.registry->AddComponent<MoveIntentComponent>(client->playerEntityID, { direction });
		return CommandResult::Success();
	}
	else {
		return CommandResult::Failure("That's not a valid direction.");
	}
}

CommandResult MovementCommandHandler::HandleNorth(ClientConnection* client,
												  const std::vector<std::string>& params,
												  GameContext& ctx) {
	ctx.registry->AddComponent<MoveIntentComponent>(client->playerEntityID, { Direction::North });
	return CommandResult::Success();
}

CommandResult MovementCommandHandler::HandleSouth(ClientConnection* client,
												  const std::vector<std::string>& params,
												  GameContext& ctx) {
	ctx.registry->AddComponent<MoveIntentComponent>(client->playerEntityID, { Direction::South });
	return CommandResult::Success();
}

CommandResult MovementCommandHandler::HandleEast(ClientConnection* client,
												 const std::vector<std::string>& params,
												 GameContext& ctx) {
	ctx.registry->AddComponent<MoveIntentComponent>(client->playerEntityID, { Direction::East });
	return CommandResult::Success();
}

CommandResult MovementCommandHandler::HandleWest(ClientConnection* client,
												 const std::vector<std::string>& params,
												 GameContext& ctx) {
	ctx.registry->AddComponent<MoveIntentComponent>(client->playerEntityID, { Direction::West });
	return CommandResult::Success();
}

CommandResult MovementCommandHandler::HandleUp(ClientConnection* client,
											   const std::vector<std::string>& params,
											   GameContext& ctx) {
	ctx.registry->AddComponent<MoveIntentComponent>(client->playerEntityID, { Direction::Up });
	return CommandResult::Success();
}

CommandResult MovementCommandHandler::HandleDown(ClientConnection* client,
												 const std::vector<std::string>& params,
												 GameContext& ctx) {
	ctx.registry->AddComponent<MoveIntentComponent>(client->playerEntityID, { Direction::Down });
	return CommandResult::Success();
}

CommandResult MovementCommandHandler::HandleClimb(ClientConnection* client,
												  const std::vector<std::string>& params,
												  GameContext& ctx) {
	// Climb defaults to up direction
	ctx.registry->AddComponent<MoveIntentComponent>(client->playerEntityID, { Direction::Up });
	return CommandResult::Success();
}
