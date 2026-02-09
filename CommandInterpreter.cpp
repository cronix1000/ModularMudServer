#include "CommandInterpreter.h"
#include "Direction.h"
#include "ClientConnection.h"
#include "MoveIntentComponent.h"
#include <map>
#include "Registry.h"
#include "NameComponent.h"
#include "PositionComponent.h"
#include "AttackIntentComponent.h"
#include "SkillHolderComponent.h"
#include "SkillIntentComponent.h"
#include "PickupItemIntentComponent.h"
#include "EquipItemIntentComponent.h"
#include "GameContext.h"
#include "WorldManager.h"
#include "GameEngine.h"
#include "MenuState.h"


CommandInterpreter::CommandInterpreter(GameContext& g) : ctx(g)
{
	RegisterCommands();
}

CommandInterpreter::~CommandInterpreter()
{
}



// Helper: Chebyshev distance
int GetDistance(int x1, int y1, int x2, int y2) {
	return (std::max)(std::abs(x1 - x2), std::abs(y1 - y2));
}

// Helper: Find target by name in the same room
// Returns -1 if not found
EntityID FindTarget(GameContext& ctx, EntityID playerID, const std::string& targetName) {
	auto* playerPos = ctx.registry->GetComponent<PositionComponent>(playerID);
	if (!playerPos) return -1;

	// This is the new pattern for iterating entities with multiple components.
	auto& name_entities = ctx.registry->view<NameComponent>();
	auto& pos_entities = ctx.registry->view<PositionComponent>();

	// Iterate over the smaller of the two sets for efficiency.
	if (name_entities.size() < pos_entities.size()) {
		for (EntityID id : name_entities) {
			if (id == playerID) continue;
			if (ctx.registry->HasComponent<PositionComponent>(id)) {
				auto* name = ctx.registry->GetComponent<NameComponent>(id);
				auto* pos = ctx.registry->GetComponent<PositionComponent>(id);
				if (pos->roomId == playerPos->roomId && name->Matches(targetName)) {
					return id;
				}
			}
		}
	}
	else {
		for (EntityID id : pos_entities) {
			if (id == playerID) continue;
			if (ctx.registry->HasComponent<NameComponent>(id)) {
				auto* name = ctx.registry->GetComponent<NameComponent>(id);
				auto* pos = ctx.registry->GetComponent<PositionComponent>(id);
				if (pos->roomId == playerPos->roomId && name->Matches(targetName)) {
					return id;
				}
			}
		}
	}

	return -1; // Not found
}
void CommandInterpreter::Interpret(ClientConnection* client, Command* command) {
	if (core_command_map_[command->CommandString])
	{
		core_command_map_[command->CommandString](client, command->Parameters, {});
	}
}
void CommandInterpreter::RegisterCommands() {
	core_command_map_["quit"] = std::bind(&CommandInterpreter::HandleQuit, this,
		std::placeholders::_1,
		std::placeholders::_2);

	core_command_map_["attack"] = std::bind(&CommandInterpreter::HandleAttack, this,
		std::placeholders::_1, std::placeholders::_2);

	// add move commands
	core_command_map_["move"] = std::bind(&CommandInterpreter::HandleMove, this,
		std::placeholders::_1, std::placeholders::_2);
	// for up and down
	core_command_map_["climb"] = std::bind(&CommandInterpreter::HandleMove, this,
		std::placeholders::_1, std::placeholders::_2);

	std::vector<std::string> northParams = { "north" };
	core_command_map_["north"] = std::bind(&CommandInterpreter::HandleMove, this,
		std::placeholders::_1, northParams);
	
	std::vector<std::string> southParams = { "south" };
	core_command_map_["south"] = std::bind(&CommandInterpreter::HandleMove, this,
		std::placeholders::_1, southParams);
	
	std::vector<std::string> eastParams = { "east" };
	core_command_map_["east"] = std::bind(&CommandInterpreter::HandleMove, this,
		std::placeholders::_1, eastParams);
	
	std::vector<std::string> westhParams = { "west" };
	core_command_map_["west"] = std::bind(&CommandInterpreter::HandleMove, this,
		std::placeholders::_1, westhParams);

	std::vector<std::string> upParams = { "up" };
	core_command_map_["up"] = std::bind(&CommandInterpreter::HandleMove, this,
		std::placeholders::_1, upParams);

	std::vector<std::string> downParams = { "down" };
	core_command_map_["down"] = std::bind(&CommandInterpreter::HandleMove, this,
		std::placeholders::_1, downParams);

	core_command_map_["kill"] = std::bind(&CommandInterpreter::HandleAttack, this,
		std::placeholders::_1, std::placeholders::_2);

	core_command_map_["pickup"] = std::bind(&CommandInterpreter::HandlePickup, this,
		std::placeholders::_1, std::placeholders::_2);

	std::vector<std::string> inventoryParams = { "inventory" };
	core_command_map_["inventory"] = std::bind(&CommandInterpreter::HandleMenu, this,
		std::placeholders::_1, inventoryParams);

	core_command_map_["equip"] = std::bind(&CommandInterpreter::HandleEquip, this,
		std::placeholders::_1, std::placeholders::_2);

	core_command_map_["cast"] = std::bind(&CommandInterpreter::HandleCast, this,
		std::placeholders::_1, std::placeholders::_2);

	core_command_map_["use"] = std::bind(&CommandInterpreter::HandleCast, this,
		std::placeholders::_1, std::placeholders::_2);

	core_command_map_["cast"] = std::bind(&CommandInterpreter::HandleCast, this,
		std::placeholders::_1, std::placeholders::_2);

	core_command_map_["use"] = std::bind(&CommandInterpreter::HandleCast, this,
		std::placeholders::_1, std::placeholders::_2);

	// Interactable commands
	core_command_map_["interact"] = std::bind(&CommandInterpreter::HandleInteract, this,
		std::placeholders::_1, std::placeholders::_2);
	core_command_map_["enter"] = std::bind(&CommandInterpreter::HandleInteract, this,
		std::placeholders::_1, std::placeholders::_2);
	core_command_map_["open"] = std::bind(&CommandInterpreter::HandleInteract, this,
		std::placeholders::_1, std::placeholders::_2);
	core_command_map_["pull"] = std::bind(&CommandInterpreter::HandleInteract, this,
		std::placeholders::_1, std::placeholders::_2);
	core_command_map_["pray"] = std::bind(&CommandInterpreter::HandleInteract, this,
		std::placeholders::_1, std::placeholders::_2);

}

void CommandInterpreter::HandleQuit(ClientConnection* client, std::vector<std::string> input)
{
	printf("Client requested QUIT. Player ID: %d\n", client->playerId); 
	client->QueueMessage("Goodbye! The world will miss you.\r\n");
	client->DisconnectGracefully();
}

void CommandInterpreter::HandleAttack(ClientConnection* client, std::vector<std::string> input)
{
	if (input.empty()) {
		client->QueueMessage("Attack who?\r\n");
		return;
	}

	// 1. Reconstruct Target Name
	std::string targetName = "";
	for (size_t i = 0; i < input.size(); ++i) {
		targetName += input[i];
		if (i < input.size() - 1) targetName += " ";
	}

	EntityID playerID = client->playerEntityID;

	// 2. Find Skill ID for "attack"
	auto* skillHolder = ctx.registry->GetComponent<SkillHolderComponent>(playerID);
	if (!skillHolder) {
		client->QueueMessage("You don't know how to fight!\r\n");
		return;
	}

	// "attack" is a reserved alias for the primary weapon skill
	int skillID = skillHolder->Lookup("attack");
	if (skillID == -1) {
		client->QueueMessage("You have no attack skill ready.\r\n");
		return;
	}

	// 3. Find Target
	EntityID targetID = FindTarget(ctx, playerID, targetName);

	if (targetID != -1) {
		ctx.registry->AddComponent<SkillIntentComponent>(playerID, { skillID, targetID });
	}
	else {
		client->QueueMessage("You don't see any '" + targetName + "' here.\r\n");
	}
}

// Handles "cast <skill> <target>" or "use <skill> <target>"
void CommandInterpreter::HandleCast(ClientConnection* client, std::vector<std::string> input) {
	if (input.empty()) {
		client->QueueMessage("Cast what?\r\n");
		return;
	}

	EntityID playerID = client->playerEntityID;
	auto* skillHolder = ctx.registry->GetComponent<SkillHolderComponent>(playerID);
	if (!skillHolder) {
		client->QueueMessage("You don't know any skills.\r\n");
		return;
	}

	// Parsing: Assume format "cast <skillname> <targetname>"
	// This is tricky if skill names have spaces (e.g., "heavy slam"). 
	// For now, we assume simple 1-word skills OR we iterate to match.

	std::string skillName = input[0];
	std::string targetName = "";

	// Simple 1-word skill logic:
	if (input.size() > 1) {
		for (size_t i = 1; i < input.size(); ++i) {
			targetName += input[i];
			if (i < input.size() - 1) targetName += " ";
		}
	}

	// 1. Lookup Skill
	int skillID = skillHolder->Lookup(skillName);
	if (skillID == -1) {
		client->QueueMessage("You don't know a skill named '" + skillName + "'.\r\n");
		return;
	}

	// 2. Lookup Target (Optional - self cast if empty?)
	EntityID targetID = -1;
	if (targetName.empty() || targetName == "self") {
		targetID = playerID;
	}
	else {
		targetID = FindTarget(ctx, playerID, targetName);
	}

	if (targetID != -1) {
		// 3. Create Intent
		ctx.registry->AddComponent<SkillIntentComponent>(playerID, { skillID, targetID });
	}
	else {
		client->QueueMessage("You don't see '" + targetName + "' here.\r\n");
	}
}
void CommandInterpreter::HandleMove(ClientConnection* client, std::vector<std::string> input)
{
	static const std::map<std::string, Direction> directionMap = {
		{"north", Direction::North},
		{"south", Direction::South},
		{"east", Direction::East},
		{"west", Direction::West},
		{"up", Direction::Up},
		{"down", Direction::Down}
	};

	Direction direction = Direction::None;
	if (!input.empty()) {
		auto it = directionMap.find(input[0]);
		if (it != directionMap.end()) {
			direction = it->second;
		}
	}

	if (direction != Direction::None) {
		ctx.registry->AddComponent<MoveIntentComponent>(client->playerEntityID, { direction });
	} else {
		// Optional: Send a message if the direction is invalid.
		client->QueueMessage("That's not a valid direction.\r\n");
	}
}

void CommandInterpreter::HandlePickup(ClientConnection* client, std::vector<std::string> input){
	if (input.empty()) {
		client->QueueMessage("Pickup what?\r\n");
		return;
	}

	std::string targetName = "";
	for (size_t i = 0; i < input.size(); ++i) {
		targetName += input[i];
		if (i < input.size() - 1) targetName += " ";
	}
	
	EntityID playerID = client->playerEntityID;
	auto* playerPos = ctx.registry->GetComponent<PositionComponent>(playerID);
	if (!playerPos) return;

	// Use the dedicated FindTarget function.
	EntityID targetID = FindTarget(ctx, playerID, targetName);

	if (targetID != -1) {
		ctx.registry->AddComponent<PickupItemIntentComponent>(playerID, {targetID});
	} else {
		client->QueueMessage("You don't see that here.\r\n");
	}
}


void CommandInterpreter::HandleEquip(ClientConnection* client, std::vector<std::string> params) {
	if (params.empty()) return;

	int actualID = std::stoi(params[0]);

	// Just fire the intent
	ctx.registry->AddComponent<EquipItemIntentComponent>(
		client->playerEntityID,
		EquipItemIntentComponent{ actualID }
	);

	client->QueueMessage("You prepare to equip the item.\n");
}
void CommandInterpreter::HandleMenu(ClientConnection* client, std::vector<std::string> input) {
	if (input[0] == "inventory")
	{
		client->PushState(new MenuState(ctx, MenuType::Inventory, client->playerEntityID));
	}
}

void CommandInterpreter::HandleInteract(ClientConnection* client, std::vector<std::string> input) {
}