#include "SaveSystem.h"
#include "GameContext.h"
#include "SQLiteDatabase.h"
#include "Registry.h"
#include "DirtyFlagComponents.h"

void SaveSystem::Run(float deltaTime) {
    saveTimer += deltaTime;

    if (saveTimer >= SAVE_INTERVAL) {
         SaveDirtyEntities();
        saveTimer = 0.0f;
    }
}


void SaveSystem::SaveDirtyEntities() {
    // Save entities whose stats have changed.
    for (EntityID entity : ctx.registry.view<StatsChangedComponent>()) {
        ctx.db.SaveStats(entity, ctx);
        ctx.registry.RemoveComponent<StatsChangedComponent>(entity);
    }

    // Save entities whose inventory has changed.
    for (EntityID entity : ctx.registry.view<InventoryChangedComponent>()) {
        ctx.db.SaveInventory(entity, ctx);
        ctx.registry.RemoveComponent<InventoryChangedComponent>(entity);
    }

    // Save entities whose body mutations have changed.
    for (EntityID entity : ctx.registry.view<MutationsChangedComponent>()) {
        ctx.db.SaveBodyMods(entity, ctx);
        ctx.registry.RemoveComponent<MutationsChangedComponent>(entity);
    }
}
