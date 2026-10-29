#include "InteractionSystem.h"
#include "GameContext.h"
#include "Registry.h"
#include "Component.h"
#include "InteractableContext.h"
#include "InteractableFactory.h"
#include "ClientConnection.h"
#include "ScriptManager.h"
#include "FactoryManager.h"
#include "WorldManager.h"
#include "ItemScriptContext.h"
#include "ItemScriptResult.h"
#include "EventBus.h"
#include <algorithm>

RoomRulesComponent* FindRoomRulesComponent(Registry& registry, int roomId) {
	for (EntityID e : registry.view<RoomIdentityComponent>()) {
		auto* id = registry.GetComponent<RoomIdentityComponent>(e);
		if (id && id->roomId == roomId) {
			return registry.GetComponent<RoomRulesComponent>(e);
		}
	}
	return nullptr;
}

InteractionSystem::~InteractionSystem() {}

void InteractionSystem::run() {
	// Handle pickup item intents
	for (EntityID entity : ctx.registry->view<PickupItemIntentComponent>()) {
		auto* intent = ctx.registry->GetComponent<PickupItemIntentComponent>(entity);
		if (!intent) continue;

		InventoryComponent* inventory = ctx.registry->GetComponent<InventoryComponent>(entity);
		ClientComponent* client = ctx.registry->GetComponent<ClientComponent>(entity);

		// Ensure the entity has the necessary components.
		if (!inventory || !client) {
			ctx.registry->RemoveComponent<PickupItemIntentComponent>(entity);
			continue;
		}

		if (inventory->items.size() < inventory->max_slots) {
			inventory->items.push_back(intent->itemID);
			client->client->QueueMessage("Item added. Inventory space left: " + std::to_string((inventory->max_slots - inventory->items.size())) + ". Use 'inventory' to view.");

			// Remove the item from the world by removing its position.
			if (ctx.registry->HasComponent<PositionComponent>(intent->itemID)) {
				ctx.registry->RemoveComponent<PositionComponent>(intent->itemID);
			}

			// Mark player as dirty to sync client.
			ctx.registry->AddComponent<PositionChangedComponent>(entity);
			ctx.registry->AddComponent<PlayerDirtyComponent>(entity);
		} else {
			client->client->QueueMessage("Inventory Full");
		}

		// Remove the intent now that it has been handled.
		ctx.registry->RemoveComponent<PickupItemIntentComponent>(entity);
	}

	// Handle drop item intents
	for (EntityID entity : ctx.registry->view<DropItemIntentComponent>()) {
		auto* intent = ctx.registry->GetComponent<DropItemIntentComponent>(entity);
		if (!intent) {
			ctx.registry->RemoveComponent<DropItemIntentComponent>(entity);
			continue;
		}

		PositionComponent* playerPos = ctx.registry->GetComponent<PositionComponent>(entity);
		ClientComponent* client = ctx.registry->GetComponent<ClientComponent>(entity);
		if (!playerPos || !client) {
			ctx.registry->RemoveComponent<DropItemIntentComponent>(entity);
			continue;
		}

		RoomRulesComponent* roomRules = FindRoomRulesComponent(*ctx.registry, playerPos->roomId);
		if (roomRules && !roomRules->allowDrop) {
			client->client->QueueMessage("You can't drop anything here.");
			ctx.registry->RemoveComponent<DropItemIntentComponent>(entity);
			continue;
		}
		if (roomRules && roomRules->maxItemsOnGround >= 0) {
			int count = 0;
			for (EntityID e : ctx.registry->view<PositionComponent>()) {
				auto* p2 = ctx.registry->GetComponent<PositionComponent>(e);
				if (p2 && p2->roomId == playerPos->roomId) count++;
			}
			if (count >= roomRules->maxItemsOnGround) {
				client->client->QueueMessage("There's no room to drop that here.");
				ctx.registry->RemoveComponent<DropItemIntentComponent>(entity);
				continue;
			}
		}

		// Remove from inventory if present.
		auto* inv = ctx.registry->GetComponent<InventoryComponent>(entity);
		if (inv) {
			auto it = std::find(inv->items.begin(), inv->items.end(), intent->itemID);
			if (it != inv->items.end()) inv->items.erase(it);
		}
		// Defensive: also clear any equipment slot holding this item.
		// Unequip intent should have already run, but order is not guaranteed.
		auto* equip = ctx.registry->GetComponent<EquipmentComponent>(entity);
		if (equip) {
			for (auto it = equip->slots.begin(); it != equip->slots.end();) {
				if (it->second == intent->itemID) it = equip->slots.erase(it);
				else ++it;
			}
		}

		// Place item on the ground at the player's tile.
		if (ctx.registry->HasComponent<PositionComponent>(intent->itemID)) {
			auto* itemPos = ctx.registry->GetComponent<PositionComponent>(intent->itemID);
			if (itemPos) {
				itemPos->x = playerPos->x;
				itemPos->y = playerPos->y;
				itemPos->roomId = playerPos->roomId;
			}
		} else {
			ctx.registry->AddComponent<PositionComponent>(intent->itemID,
				PositionComponent{ playerPos->x, playerPos->y, playerPos->roomId });
		}

		// Lua hook: per-item on_drop script (cancellable).
		bool cancel = false;
		if (ctx.scripts) {
			auto* scriptComp = ctx.registry->GetComponent<ScriptComponent>(intent->itemID);
			if (scriptComp) {
				auto it = scriptComp->scripts_path.find("on_drop");
				if (it != scriptComp->scripts_path.end() && !it->second.empty()) {
					ItemScriptContext isctx;	
					isctx.userID = entity;
					isctx.itemID = intent->itemID;
					isctx.roomID = playerPos->roomId;
					isctx.x = playerPos->x;
					isctx.y = playerPos->y;
					auto* itemC = ctx.registry->GetComponent<ItemComponent>(intent->itemID);
					if (itemC) isctx.itemTemplateID = itemC->templateName;
					ItemScriptResult res = ctx.scripts->ExecuteItemScript(it->second, "on_execute", isctx);
					if (!res.message.empty()) {
						client->client->QueueMessage(res.message);
					}
					if (res.cancelDrop) cancel = true;
				}
			}
		}

		if (cancel) {
			// Revert: remove the ground position, push back to inventory.
			ctx.registry->RemoveComponent<PositionComponent>(intent->itemID);
			if (inv) inv->items.push_back(intent->itemID);
			ctx.registry->RemoveComponent<DropItemIntentComponent>(entity);
			continue;
		}

		EventContext evt;
		evt.data = ItemDroppedEventData{ entity, intent->itemID, playerPos->roomId };
		ctx.eventBus->Publish(EventType::ItemDropped, evt);

		ctx.registry->AddComponent<InventoryChangedComponent>(entity);
		ctx.registry->AddComponent<PlayerDirtyComponent>(entity);

		ctx.registry->RemoveComponent<DropItemIntentComponent>(entity);
	}

	// Handle interactable intents
	for (EntityID entity : ctx.registry->view<InteractableIntentComponent>()) {
		auto* intent = ctx.registry->GetComponent<InteractableIntentComponent>(entity);
		if (!intent) continue;

		ClientComponent* client = ctx.registry->GetComponent<ClientComponent>(entity);
		if (!client) {
			ctx.registry->RemoveComponent<InteractableIntentComponent>(entity);
			continue;
		}  

		auto* inter = ctx.registry->GetComponent<InteractableComponent>(intent->interactEntityId);
		auto* scriptComp = ctx.registry->GetComponent<ScriptComponent>(intent->interactEntityId);
		auto* posComp = ctx.registry->GetComponent<PositionComponent>(intent->interactEntityId);
		if (!inter || !posComp) {
			client->client->QueueMessage("You cannot interact with that.");
			ctx.registry->RemoveComponent<InteractableIntentComponent>(entity);
			continue;
		}
		if (!scriptComp) {
			client->client->QueueMessage("That object is not interactive.");
			ctx.registry->RemoveComponent<InteractableIntentComponent>(entity);
			continue;
		}
		std::string scriptPath = scriptComp->scripts_path["on_use"];
		if (scriptPath.empty()) {
			client->client->QueueMessage("That object is not interactive.");
			ctx.registry->RemoveComponent<InteractableIntentComponent>(entity);	
			continue;
		}

		InteractableContext context{
			intent->interactEntityId, entity, posComp->roomId, posComp->x, posComp->y, intent->action, intent->parameters
		};

		auto result = ctx.scripts->ExecuteInteractableScript(scriptPath, "on_use", context);

		if (result.success) {
			// Send message to user
			if (!result.message.empty()) {
				client->client->QueueMessage(result.message);
			}

			// Send message to room (if different from user message)
			if (!result.roomMessage.empty()) {
				// TODO: Send to all players in room except the user
				// This would require a room messaging system
			}

			// Handle specific action types
			HandleInteractableResult(entity, result);
		} else {
			client->client->QueueMessage("You cannot do that right now.");
		}

		// Remove the intent now that it has been handled
		ctx.registry->RemoveComponent<InteractableIntentComponent>(entity);
	}
}

void InteractionSystem::HandleInteractableResult(int userEntityID, const InteractableResult& result) {
	switch (result.actionType[0]) {  // Simple switch on first character for performance
	case 't': // "teleport"
		if (result.actionType == "teleport" && result.targetRoomID != -1) {
			auto* pos = ctx.registry->GetComponent<PositionComponent>(userEntityID);
			if (pos && ctx.worldManager) {
				ctx.worldManager->AttemptTeleport(pos, result.targetRoomID);
				ctx.registry->AddComponent<PositionChangedComponent>(userEntityID);
			}
		}
		break;

	case 's': // "spawn_item"
		if (result.actionType == "spawn_item" && !result.spawnItemID.empty()) {
			// Use item factory to spawn the item
			if (ctx.factories) {
				auto* userPos = ctx.registry->GetComponent<PositionComponent>(userEntityID);
				if (userPos) {
					int spawnX = (result.spawnX != -1) ? result.spawnX : userPos->x;
					int spawnY = (result.spawnY != -1) ? result.spawnY : userPos->y;
					ctx.factories->items.CreateItem(
						result.spawnItemID,
						json::object(),
						spawnX,
						spawnY,
						userPos->roomId
					);
				}
			}
		}
		break;

	case 'h': // "heal_player"
		if (result.actionType == "heal_player") {
			auto* stats = ctx.registry->GetComponent<StatComponent>(userEntityID);
			if (stats && result.targetX > 0) {  // Using targetX as heal amount
				stats->Health = (std::min)(stats->Health + result.targetX, stats->MaxHealth);
				ctx.registry->AddComponent<VitalsChangedComponent>(userEntityID);
			}
		}
		break;

	case 'e': // "trigger_event"
		if (result.actionType == "trigger_event" && !result.eventName.empty()) {
			// Trigger the event through the script system
			sol::table eventData = ctx.scripts->lua.create_table();
			eventData["event_name"] = result.eventName;
			eventData["user_id"] = userEntityID;

			// Add event parameters
			for (size_t i = 0; i < result.eventParams.size(); ++i) {
				eventData["param_" + std::to_string(i)] = result.eventParams[i];
			}

			ctx.scripts->dispatch_event(result.eventName, eventData);

		}
		break;
	}
}