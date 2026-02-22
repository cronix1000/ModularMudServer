#include "InteractionSystem.h"
#include "GameContext.h"     
#include "Registry.h"      
#include "Component.h"
#include "InteractableIntentComponent.h"
#include "InteractableContext.h"
#include "InteractableFactory.h"
#include "ClientConnection.h"
#include "ScriptManager.h"
#include "FactoryManager.h"
#include "WorldManager.h"

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
			ctx.registry->AddComponent<InventoryChangedComponent>(entity);
		} else {
			client->client->QueueMessage("Inventory Full");
		}

		// Remove the intent now that it has been handled.
		ctx.registry->RemoveComponent<PickupItemIntentComponent>(entity);
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

		// Use ScriptManager to handle the interaction
		auto* scriptComp = ctx.registry->GetComponent<ScriptComponent>(intent->interactableID);
		auto* posComp = ctx.registry->GetComponent<PositionComponent>(intent->interactableID);
		
		if (!scriptComp || !posComp) {
			client->client->QueueMessage("You cannot interact with that.");
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
			intent->interactableID, entity, posComp->roomId, posComp->x, posComp->y, intent->action, intent->parameters
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
			if (pos) {
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
				stats->Health = (std::min)(stats->Health + result.targetX, stats->Health);
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