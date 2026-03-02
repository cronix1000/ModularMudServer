#include "ItemCommandHandler.h"
#include "CommandRegistry.h"
#include "ClientConnection.h"
#include "GameContext.h"
#include "Registry.h"
#include "NameComponent.h"
#include "PositionComponent.h"
#include "InventoryComponent.h"
#include "PickupItemIntentComponent.h"
#include "EquipItemIntentComponent.h"
#include "MenuState.h"

EntityID ItemCommandHandler::FindItemByName(GameContext& ctx, EntityID playerID, const std::string& itemName) {
	auto* playerPos = ctx.registry->GetComponent<PositionComponent>(playerID);
	if (!playerPos) return -1;

	auto& name_entities = ctx.registry->view<NameComponent>();

	for (EntityID id : name_entities) {
		if (id == playerID) continue;
		
		if (ctx.registry->HasComponent<PositionComponent>(id)) {
			auto* name = ctx.registry->GetComponent<NameComponent>(id);
			auto* pos = ctx.registry->GetComponent<PositionComponent>(id);
			
			// Item must be on the ground (no inventory component) and in same room
			if (pos->roomId == playerPos->roomId && 
				!ctx.registry->HasComponent<InventoryComponent>(id) &&
				name->Matches(itemName)) {
				return id;
			}
		}
	}

	return -1;
}

std::string ItemCommandHandler::BuildItemName(const std::vector<std::string>& params) {
	if (params.empty()) return "";
	
	std::string itemName = "";
	for (size_t i = 0; i < params.size(); ++i) {
		itemName += params[i];
		if (i < params.size() - 1) itemName += " ";
	}
	return itemName;
}

void ItemCommandHandler::RegisterAll(CommandRegistry& registry) {
	// Item manipulation
	registry.RegisterWithAliases("pickup", HandlePickup, {"get", "take"}, PermissionLevel::Player);
	registry.RegisterWithAliases("drop", HandleDrop, {}, PermissionLevel::Player);
	registry.RegisterWithAliases("equip", HandleEquip, {"wear", "wield"}, PermissionLevel::Player);
	registry.RegisterWithAliases("inventory", HandleInventory, {"i", "inv"}, PermissionLevel::Player);
}

CommandResult ItemCommandHandler::HandlePickup(ClientConnection* client,
											   const std::vector<std::string>& params,
											   GameContext& ctx) {
	if (params.empty()) {
		return CommandResult::Failure("Pickup what?");
	}

	EntityID playerID = client->playerEntityID;
	std::string itemName = BuildItemName(params);

	EntityID targetID = FindItemByName(ctx, playerID, itemName);

	if (targetID != -1) {
		ctx.registry->AddComponent<PickupItemIntentComponent>(playerID, { targetID });
		return CommandResult::Success();
	}
	else {
		return CommandResult::Failure("You don't see that here.");
	}
}

CommandResult ItemCommandHandler::HandleDrop(ClientConnection* client,
											 const std::vector<std::string>& params,
											 GameContext& ctx) {
	// TODO: Implement drop functionality
	return CommandResult::Failure("Drop command not yet implemented.");
}

CommandResult ItemCommandHandler::HandleEquip(ClientConnection* client,
											  const std::vector<std::string>& params,
											  GameContext& ctx) {
	if (params.empty()) {
		return CommandResult::Failure("Equip what?");
	}

	try {
		int actualID = std::stoi(params[0]);
		
		ctx.registry->AddComponent<EquipItemIntentComponent>(
			client->playerEntityID,
			EquipItemIntentComponent{ actualID }
		);

		return CommandResult::Success("You prepare to equip the item.");
	}
	catch (const std::exception& e) {
		return CommandResult::Failure("Invalid item ID. Please provide a valid number.");
	}
}

CommandResult ItemCommandHandler::HandleInventory(ClientConnection* client,
												  const std::vector<std::string>& params,
												  GameContext& ctx) {
	// Switch to inventory menu state
	client->PushState(new MenuState(ctx, MenuType::Inventory, client->playerEntityID));
	return CommandResult::Success();
}
