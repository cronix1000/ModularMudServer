#include "SaveSystem.h"
#include "IDatabase.h"
#include "GameContext.h"
#include "Registry.h"
#include "DirtyFlagComponents.h"

SaveSystem::~SaveSystem() {
    // Destructor - nothing special to clean up
}

void SaveSystem::Run(float deltaTime) {
    saveTimer += deltaTime;

    if (saveTimer >= SAVE_INTERVAL) {
         SaveDirtyEntities();
        saveTimer = 0.0f;
    }
}

void SaveSystem::SaveNow(EntityID entityID) {
    ctx.db->SavePlayer(entityID, ctx);

}

void SaveSystem::SaveAllPlayers() {

}

void SaveSystem::SaveDirtyEntities() {
    std::vector<EntityID> dirty;
    // Save entities whose stats have changed
    for (EntityID entityID : ctx.registry->view<PlayerDirtyComponent>()) {
        dirty.push_back(entityID);
    }
    for (EntityID entityID : dirty) {
        ctx.db->SavePlayer(entityID, ctx);
        ctx.registry->RemoveComponent<PlayerDirtyComponent>(entityID);
    }

}
