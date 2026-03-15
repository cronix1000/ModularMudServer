#include "SaveSystem.h"
#include "GameContext.h"
#include "SQLiteDatabase.h"
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


void SaveSystem::SaveDirtyEntities() {
    // Save entities whose stats have changed
    for (EntityID entityID : ctx.registry->view<PlayerDirtyComponent>()) {
        ctx.db->SavePlayer(entityID, ctx);
    }
}
