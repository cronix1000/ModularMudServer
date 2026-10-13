#include "ItemCommandHandler.h"
#include "CommandRegistry.h"
#include "ClientConnection.h"
#include "GameContext.h"
#include "Registry.h"
#include "NameComponent.h"
#include "PositionComponent.h"
#include "InventoryComponent.h"
#include "EquipmentComponent.h"
#include "PickupItemIntentComponent.h"
#include "DropItemIntentComponent.h"
#include "EquipItemIntentComponent.h"
#include "ItemComponent.h"
#include "TargetingIntentComponent.h"
#include "ArmourComponent.h"
#include "WeaponComponent.h"
#include "TextHelperFunctions.h"
#include "EquipmentSlotInfo.h"
#include "EntityResolver.h"
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>


void ItemCommandHandler::RegisterAll(CommandRegistry& registry) {
	registry.RegisterWithAliases("pickup", HandlePickup, { "get", "take" }, PermissionLevel::Player);
	registry.RegisterWithAliases("drop", HandleDrop, {}, PermissionLevel::Player);
	registry.RegisterWithAliases("equip", HandleEquip, { "wear", "wield" }, PermissionLevel::Player);
	registry.RegisterWithAliases("inventory", HandleInventory, { "i", "inv" }, PermissionLevel::Player);
	registry.RegisterWithAliases("equipment", HandleEquipment, { "eq" }, PermissionLevel::Player);
	registry.RegisterWithAliases("unequip", HandleUnequip, { "remove" }, PermissionLevel::Player);
}

CommandResult ItemCommandHandler::HandlePickup(ClientConnection* client,
	const std::vector<std::string>& params,
	GameContext& ctx) {
	if (params.empty()) {
		return CommandResult::Failure("Pickup what?");
	}

	EntityID playerID = client->playerEntityID;
	auto* playerPos = ctx.registry->GetComponent<PositionComponent>(playerID);
	if (!playerPos) {
		return CommandResult::Failure("You are nowhere.");
	}

	auto hits = ctx.entityFind->FindByShape(
		playerPos->roomId, playerPos->x, playerPos->y,
		params, EntityResolver::ShapeAround());

	if (hits.empty()) {
		return CommandResult::Failure("You don't see that here.");
	}

	if (hits.size() > 1) {
		TargetingIntentComponent intent;
		intent.sourceID = playerID;
		intent.targetName = "";
		intent.candidates = hits;
		intent.queryTokens = params;
		intent.roomId = playerPos->roomId;
		intent.promptExpireTime = 30.0f;
		intent.elapsedTime = 0.0f;
		ctx.registry->AddComponent<TargetingIntentComponent>(playerID, intent);

		std::ostringstream out;
		out << "Which one did you mean?\r\n";
		for (size_t i = 0; i < hits.size(); ++i) {
			auto* name = ctx.registry->GetComponent<NameComponent>(hits[i]);
			out << "  " << (i + 1) << ". " << (name ? name->displayName : "something") << "\r\n";
		}
		out << "Pick 1-" << hits.size() << " or refine your search.\r\n";
		return CommandResult::Success(out.str());
	}

	EntityID target = hits[0];
	if (!ctx.registry->HasComponent<ItemComponent>(target)) {
		return CommandResult::Failure("You can't pick that up.");
	}

	ctx.registry->AddComponent<PickupItemIntentComponent>(playerID, PickupItemIntentComponent{ target });
	return CommandResult::Success();
}

CommandResult ItemCommandHandler::HandleDrop(ClientConnection* client,
	const std::vector<std::string>& params,
	GameContext& ctx) {

	if (params.empty()) {
		return CommandResult::Failure("Drop what?");
	}

	EntityID playerID = client->playerEntityID;
	auto* playerPos = ctx.registry->GetComponent<PositionComponent>(playerID);
	if (!playerPos) {
		return CommandResult::Failure("You are nowhere.");
	}

	int target = -1;

	try {
		int id = std::stoi(params[0]);
		auto* equip = ctx.registry->GetComponent<EquipmentComponent>(playerID);
		if (equip) {
			for (const auto& [s, e] : equip->slots) {
				if (e == id) { target = id; break; }
			}
		}
		if (target == -1) {
			auto* inv = ctx.registry->GetComponent<InventoryComponent>(playerID);
			if (inv) {
				for (int e : inv->items) {
					if (e == id) { target = id; break; }
				}
			}
		}
	}
	catch (...) {
		auto hits = ctx.entityFind->FindInEquipment(playerID, params);
		if (!hits.empty()) target = hits[0];
		if (target == -1) {
			auto* inv = ctx.registry->GetComponent<InventoryComponent>(playerID);
			if (inv) {
				for (int e : inv->items) {
					auto* name = ctx.registry->GetComponent<NameComponent>(e);
					if (!name) continue;
					std::string lower = name->displayName;
					std::transform(lower.begin(), lower.end(), lower.begin(),
						[](unsigned char c) { return std::tolower(c); });
					bool match = true;
					for (const auto& tok : params) {
						if (lower.find(tok) == std::string::npos) {
							match = false;
							break;
						}
					}
					if (match) { target = e; break; }
				}
			}
		}
	}

	if (target == -1) {
		return CommandResult::Failure("You don't have that.");
	}

	bool isEquipped = false;
	EquipmentSlot equippedSlot = EquipmentSlot::Default;
	auto* equip = ctx.registry->GetComponent<EquipmentComponent>(playerID);
	if (equip) {
		for (const auto& [s, e] : equip->slots) {
			if (e == target) { isEquipped = true; equippedSlot = s; break; }
		}
	}

	if (isEquipped) {
		ctx.registry->AddComponent<UnequipItemIntentComponent>(
			playerID,
			UnequipItemIntentComponent{ target, equippedSlot });
	}

	ctx.registry->AddComponent<DropItemIntentComponent>(
		playerID,
		DropItemIntentComponent{ target });
	return CommandResult::Success("You drop the item.");
}

CommandResult ItemCommandHandler::HandleEquip(ClientConnection* client,
	const std::vector<std::string>& params,
	GameContext& ctx) {
	if (params.empty()) {
		return CommandResult::Failure("Equip what?");
	}

	EntityID playerID = client->playerEntityID;
	EquipmentSlot slot = EquipmentSlot::Default;
	size_t itemStart = 0;

	if (params.size() >= 2) {
		EquipmentSlot parsed = EquipmentSlotInfo::ParseSlotString(params[0]);
		if (parsed != EquipmentSlot::Default) {
			slot = parsed;
			itemStart = 1;
		}
	}

	if (itemStart == 0) {
		try {
			int actualID = std::stoi(params[0]);
			ctx.registry->AddComponent<EquipItemIntentComponent>(
				playerID,
				EquipItemIntentComponent{ actualID, slot }
			);
			return CommandResult::Success("You prepare to equip the item.");
		}
		catch (const std::exception&) {
		}
	}

	auto* playerPos = ctx.registry->GetComponent<PositionComponent>(playerID);
	if (!playerPos) {
		return CommandResult::Failure("You are nowhere.");
	}

	std::vector<std::string> itemQuery(params.begin() + itemStart, params.end());
	auto hits = ctx.entityFind->FindByShape(
		playerPos->roomId, playerPos->x, playerPos->y,
		itemQuery, EntityResolver::ShapeAround());

	int candidateId = -1;

	auto* inv = ctx.registry->GetComponent<InventoryComponent>(playerID);
	if (inv) {
		for (int itemId : inv->items) {
			auto* name = ctx.registry->GetComponent<NameComponent>(itemId);
			if (!name) continue;
			std::string lower = name->displayName;
			std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
			bool match = true;
			for (const auto& tok : itemQuery) {
				if (lower.find(tok) == std::string::npos) {
					match = false;
					break;
				}
			}
			if (match) {
				candidateId = itemId;
				break;
			}
		}
	}

	if (candidateId == -1) {
		return CommandResult::Failure("You don't have that item.");
	}

	ctx.registry->AddComponent<EquipItemIntentComponent>(
		playerID,
		EquipItemIntentComponent{ candidateId, slot }
	);
	return CommandResult::Success("You prepare to equip the item.");
}

CommandResult ItemCommandHandler::HandleInventory(ClientConnection* client,
	const std::vector<std::string>& params,
	GameContext& ctx) {
	(void)params;
	InventoryComponent* inv = ctx.registry->GetComponent<InventoryComponent>(client->playerEntityID);
	if (!inv) {
		return CommandResult::Failure("You have no inventory.");
	}

	std::ostringstream out;
	out << "===== [ INVENTORY ] =====\r\n";
	int n = 0;
	for (auto& itemId : inv->items) {
		n++;
		auto* name = ctx.registry->GetComponent<NameComponent>(itemId);
		auto* armour = ctx.registry->GetComponent<ArmourComponent>(itemId);
		out << std::setw(3) << n << " | "
			<< std::setw(24) << (name ? name->displayName : "?") << " | "
			<< std::setw(9);
		if (armour) {
			auto* itemC = ctx.registry->GetComponent<ItemComponent>(itemId);
			if (itemC && !itemC->allowedSlots.empty()) {
				out << TextHelperFunctions::SlotToString(itemC->allowedSlots.front());
			}
			else {
				out << "General";
			}
		}
		else {
			out << "General";
		}
		out << " | item" << "\r\n";
	}
	return CommandResult::Success(out.str());
}

CommandResult ItemCommandHandler::HandleEquipment(ClientConnection* client,
	const std::vector<std::string>& params,
	GameContext& ctx) {
	(void)params;
	EntityID playerID = client->playerEntityID;
	EquipmentComponent* equipment = ctx.registry->GetComponent<EquipmentComponent>(playerID);
	if (!equipment) {
		return CommandResult::Failure("You have no equipment.");
	}

	std::ostringstream out;
	out << "===== [ EQUIPMENT ] =====\r\n";

	static const EquipmentSlot kAllSlots[] = {
		EquipmentSlot::Helmet,
		EquipmentSlot::Chest,
		EquipmentSlot::Bracers,
		EquipmentSlot::Leggings,
		EquipmentSlot::MainArm,
		EquipmentSlot::OffHand,
		EquipmentSlot::Shoulders,
		EquipmentSlot::Knees,
		EquipmentSlot::Shoes,
		EquipmentSlot::Necklace,
		EquipmentSlot::Back,
		EquipmentSlot::Waist,
		EquipmentSlot::Wrists,
		EquipmentSlot::Finger1,
		EquipmentSlot::Finger2,
		EquipmentSlot::Finger3,
		EquipmentSlot::Finger4,
		EquipmentSlot::Finger5,
		EquipmentSlot::Finger6,
		EquipmentSlot::Finger7,
		EquipmentSlot::Finger8,
		EquipmentSlot::Finger9,
		EquipmentSlot::Finger10,
	};

	for (EquipmentSlot slot : kAllSlots) {
		out << std::setw(14) << std::string(EquipmentSlotInfo::SlotToString(slot)) << " : ";
		auto it = equipment->slots.find(slot);
		if (it != equipment->slots.end() && it->second > 0) {
			auto* name = ctx.registry->GetComponent<NameComponent>(it->second);
			out << (name ? name->displayName : "(unknown)");
		}
		else {
			out << "-";
		}
		out << "\r\n";
	}

	return CommandResult::Success(out.str());
}

CommandResult ItemCommandHandler::HandleUnequip(ClientConnection* client, const std::vector<std::string>& params, GameContext& ctx)
{
	EntityID playerID = client->playerEntityID;

	if (params.size() == 0) {
		return CommandResult::Failure("Unequip what?");
	}
	int target = -1;
	EquipmentSlot slotHint = TextHelperFunctions::StringToSlot(params[0]);
	if (slotHint != EquipmentSlot::Default) {
		EquipmentComponent* equipment = ctx.registry->GetComponent<EquipmentComponent>(playerID);
		if (equipment) {
			auto it = equipment->slots.find(slotHint);
			if (it != equipment->slots.end()) target = it->second;
		}
	}
	if (target == -1) {
		auto hits = ctx.entityFind->FindInEquipment(playerID, params);
		if(hits.size() > 0) target = hits[0];
	}
	if (target == -1) {
		return CommandResult::Failure("You don't have that equipped.");
	}

	EquipmentSlot resolvedSlot = slotHint;
	if (resolvedSlot == EquipmentSlot::Default) {
		auto* equip = ctx.registry->GetComponent<EquipmentComponent>(playerID);
		if (equip) resolvedSlot = EquipmentSlotInfo::FindSlotOfEntity(*equip, target);
	}
	ctx.registry->AddComponent<UnequipItemIntentComponent>(
		playerID,
		UnequipItemIntentComponent{ target, resolvedSlot }
	);

	return CommandResult::Success("You try to unequip.");
}
