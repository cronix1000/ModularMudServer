#include "MovementSystem.h"

#include "Component.h"
#include "Direction.h"
#include "EventBus.h"
#include "GameContext.h"
#include "MoveIntentComponent.h"
#include "PositionComponent.h"
#include "Registry.h"
#include "ScriptManager.h"
#include "SkillContext.h"
#include "TextHelperFunctions.h"
#include "World.h"
#include "WorldManager.h"
#include "RoomComponents.h"

#include <vector>

MovementSystem::MovementSystem(GameContext& c) : ctx(c) {
}

MovementSystem::~MovementSystem() {
}

void MovementSystem::MovementSystemRun() {
	// The new `view` gives us a list of all entities with a MoveIntentComponent.
	// This is much more efficient as it allows us to iterate over tightly packed data.
	for (EntityID entityId : ctx.registry->view<MoveIntentComponent>()) {
		MoveIntentComponent* intent = ctx.registry->GetComponent<MoveIntentComponent>(entityId);
		PositionComponent* posComponent = ctx.registry->GetComponent<PositionComponent>(entityId);

		if (!posComponent || !intent) continue;

		bool moved = false;

		// Portal logic...
		for (EntityID portalId : ctx.registry->view<PortalComponent>()) {
			auto portalPos = ctx.registry->GetComponent<PositionComponent>(portalId);
			if (portalPos && portalPos->roomId == posComponent->roomId && portalPos->x == posComponent->x && portalPos->y == posComponent->y) {
				auto portal = ctx.registry->GetComponent<PortalComponent>(portalId);
				if (portal && TextHelperFunctions::StringToDirection(portal->direction_command) == intent->direction) {
					ctx.worldManager->AttemptMove(intent->direction, posComponent, entityId);
					moved = true;
					break;
				}
			}
		}

		if (!moved) {
			int result = ctx.worldManager->AttemptMove(intent->direction, posComponent, entityId);

			if (result == 1) { // Normal move
				ctx.registry->AddComponent<PositionChangedComponent>(entityId);
			} else if (result == 2) { // Room change
				ctx.eventBus->Publish(EventType::RoomEntered, {RoomEventData{entityId, posComponent->roomId}});
				ctx.registry->AddComponent<PositionChangedComponent>(entityId);

				// Get room entity for script execution
				EntityID roomEntity = 0;
				for (EntityID ent : ctx.registry->view<RoomIdentityComponent>()) {
					auto* identity = ctx.registry->GetComponent<RoomIdentityComponent>(ent);
					if (identity && identity->roomId == posComponent->roomId) {
						roomEntity = ent;
						break;
					}
				}
				
				if (roomEntity != 0) {
					if (auto* script = ctx.registry->GetComponent<ScriptComponent>(roomEntity)) {
						auto it = script->scripts_path.find("on_enter");
						if (it != script->scripts_path.end()) {
							ctx.scripts->execute_hook(it->second, entityId, posComponent->roomId);
						}
					}
				}
			} else { // Failed move
				if (auto* client = ctx.registry->GetComponent<ClientComponent>(entityId)) {
					client->client->QueueMessage("No Room there");
				}
			}
		}

		// Remove the intent after it has been processed.
		ctx.registry->RemoveComponent<MoveIntentComponent>(entityId);
	}
}