#include "MovementSystem.h"
#include "Direction.h"
#include "Component.h"
#include "TextHelperFunctions.h"

#include "World.h"          
#include "SkillContext.h"   
#include "WorldManager.h"    
#include "Registry.h"
#include "EventBus.h"       
#include "ScriptManager.h"  

#include "MoveIntentComponent.h"  
#include "PositionComponent.h"    
#include <vector>                 

#include "GameContext.h"


MovementSystem::MovementSystem(GameContext& c) : ctx(c)
{
}

MovementSystem::~MovementSystem()
{
}

//Direction StringToDirection(const std::string& dir) {
//    static const std::map<std::string, Direction> directionMapString = {
//       {"north", Direction::North},
//       {"south", Direction::South},
//       {"east", Direction::East},
//       {"west", Direction::West},
//       {"up", Direction::Up},
//       {"down", Direction::Down},
//    };
//
//    auto it = directionMapString.find(dir);
//    if (it != directionMapString.end()) {
//        return it->second;
//    }
//    return Direction::None;
//}

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
            }
            else if (result == 2) { // Room change
                ctx.eventBus->Publish(EventType::RoomEntered, {RoomEventData{entityId, posComponent->roomId}});
                ctx.registry->AddComponent<PositionChangedComponent>(entityId);

                if (auto* currentRoom = ctx.worldManager->world->GetRoom(posComponent->roomId)) {
                    int roomId = currentRoom->GetEnityID();
                    if (auto* script = ctx.registry->GetComponent<ScriptComponent>(roomId)) {
                        auto it = script->scripts_path.find("on_enter");
                        if (it != script->scripts_path.end()) {
                            ctx.scripts->execute_hook(it->second, entityId, posComponent->roomId);
                        }
                    }
                }
            }
            else { // Failed move
                if (auto* client = ctx.registry->GetComponent<ClientComponent>(entityId)) {
                    client->client->QueueMessage("No Room there");
                }
            }
        }
        
        // Remove the intent after it has been processed.
        ctx.registry->RemoveComponent<MoveIntentComponent>(entityId);
    }
}

