#include "CleanUpSystem.h"
#include "GameContext.h"
#include "Registry.h"
#include "Tags.h"
#include <vector>

void CleanUpSystem::run() {
    // This system finds all entities marked with a `DestroyTag` and removes them
    // from the game.

    // It's important to collect the IDs first before destroying them.
    // Calling registry.DestroyEntity() can modify the component pools while we are
    // iterating over them, which can lead to crashes.
    std::vector<EntityID> entities_to_destroy;
    for (EntityID entity : ctx.registry->view<DestroyTag>()) {
        entities_to_destroy.push_back(entity);
    }

    for (EntityID entity : entities_to_destroy) {
        // This removes the entity and all of its components from the registry.
        ctx.registry->DestroyEntity(entity);
    }
}