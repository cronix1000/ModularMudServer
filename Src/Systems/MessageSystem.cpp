#include "MessageSystem.h"
#include "ClientConnection.h"
#include "EventBus.h"
#include "GameContext.h"
#include "Registry.h"
#include "InventoryComponents.h"
#include "PlayerComponents.h"
#include "PresentationComponents.h"
#include "InteractionMessage.h"
#include "TextHelperFunctions.h"

namespace {
	std::string PlayerDisplayName(Registry& r, int playerID) {
		auto* n = r.GetComponent<NameComponent>(playerID);
		return n ? n->displayName : "someone";
	}

	EquipmentSlot SlotOfItem(Registry& r, int playerID, int itemID) {
		auto* equip = r.GetComponent<EquipmentComponent>(playerID);
		if (!equip) return EquipmentSlot::Default;
		for (const auto& [s, e] : equip->slots) {
			if (e == itemID) return s;
		}
		return EquipmentSlot::Default;
	}
}

MessageSystem::~MessageSystem() {
	// Destructor - nothing special to clean up
}

void MessageSystem::SanitizeMessage(std::string& message)
{

}

void MessageSystem::SubscribeToEvents()
{
	ctx.eventBus->Subscribe(EventType::ItemEquipped, [this](const EventContext& ectx) {
		if (!std::holds_alternative<ItemEquippedEventData>(ectx.data)) return;
		const auto& d = std::get<ItemEquippedEventData>(ectx.data);

		NameComponent* itemName = ctx.registry->GetComponent<NameComponent>(d.itemId);
		if (!itemName) return;

		std::unordered_map<std::string, std::string> tokens{
			{"name", itemName->displayName},
			{"wielder", PlayerDisplayName(*ctx.registry, d.player)},
		};
		EquipmentSlot slot = SlotOfItem(*ctx.registry, d.player, d.itemId);
		if (slot != EquipmentSlot::Default) {
			tokens["slot"] = EquipmentSlotInfo::SlotToString(slot);
		}

		auto resolved = InteractionMessage::Resolve(*ctx.registry, d.itemId, "equip", tokens);
		std::string fallback = "You equip " + itemName->displayName + ".";
		InteractionMessage::ExpandTokens(fallback, tokens);
		ToPlayer(d.player, resolved ? resolved->message : fallback);
	});

	ctx.eventBus->Subscribe(EventType::ItemUnequipped, [this](const EventContext& ectx) {
		if (!std::holds_alternative<ItemUnequippedEventData>(ectx.data)) return;
		const auto& d = std::get<ItemUnequippedEventData>(ectx.data);

		NameComponent* itemName = ctx.registry->GetComponent<NameComponent>(d.itemId);
		if (!itemName) return;

		std::unordered_map<std::string, std::string> tokens{
			{"name", itemName->displayName},
			{"slot", EquipmentSlotInfo::SlotToString(d.slot)},
			{"wielder", PlayerDisplayName(*ctx.registry, d.player)},
		};

		auto resolved = InteractionMessage::Resolve(*ctx.registry, d.itemId, "unequip", tokens);
		std::string fallback = "You remove " + itemName->displayName + ".";
		InteractionMessage::ExpandTokens(fallback, tokens);
		ToPlayer(d.player, resolved ? resolved->message : fallback);
	});

	ctx.eventBus->Subscribe(EventType::ItemDropped, [this](const EventContext& ectx) {
		if (!std::holds_alternative<ItemDroppedEventData>(ectx.data)) return;
		const auto& d = std::get<ItemDroppedEventData>(ectx.data);

		NameComponent* itemName = ctx.registry->GetComponent<NameComponent>(d.itemId);
		if (!itemName) return;

		std::unordered_map<std::string, std::string> tokens{
			{"name", itemName->displayName},
			{"wielder", PlayerDisplayName(*ctx.registry, d.player)},
		};

		auto resolved = InteractionMessage::Resolve(*ctx.registry, d.itemId, "drop", tokens);
		std::string fallback = "You drop " + itemName->displayName + ".";
		InteractionMessage::ExpandTokens(fallback, tokens);
		ToPlayer(d.player, resolved ? resolved->message : fallback);
	});
}

void MessageSystem::ToPlayer(int entityID, std::string msg)
{
	ClientComponent* client = ctx.registry->GetComponent<ClientComponent>(entityID);

	if (client) {
		client->client->QueueMessage(TextHelperFunctions::Colorize(msg));
	}

}

void MessageSystem::ToRoom(int roomID, std::string msg, int excludeID)
{
}

void MessageSystem::ToGlobal(std::string msg)
{

}