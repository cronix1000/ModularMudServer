#include "PlayerFactory.h"
#include <iostream>
#include "PlayerData.h"
#include "GameContext.h"
#include "Registry.h"
#include "SQLiteDatabase.h"
#include "ClientComponent.h"
#include "PlayerComponent.h"
#include "StatComponent.h"
#include "PositionComponent.h"
#include "InventoryComponent.h"
#include "VisualComponent.h"
#include "EquipmentComponent.h"
#include "SkillHolderComponent.h"
#include "FactoryManager.h"
#include "WorldManager.h"
#include "World.h"

EntityID PlayerFactory::LoadPlayer(std::string username, ClientConnection* connection) {
    PlayerData data;

    // 1. Check if they exist in DB
    if (!ctx.db->PlayerExists(username)) {
        printf("[Factory] Error on name", username.c_str());
        return -1;
    }

    // 2. Load the data (whether just created or old)
    if (!ctx.db->LoadPlayer(username, data)) {
        return -1;
    }

    // 3. Create ECS Entity
    EntityID player = ctx.registry->CreateEntity();

    // 4. Attach Components (Hydration)
    ctx.registry->AddComponent(player, ClientComponent{ connection });
    ctx.registry->AddComponent(player, PlayerComponent{ data.id, username });

    // Stats from DB
    auto& s = data.stats;
    StatComponent stats;
    stats.Health = s.value("hp", 100);
    stats.MaxHealth = s.value("max_hp", 100);
    stats.Strength = s.value("str", 10);
    ctx.registry->AddComponent(player, stats);

    std::string regionToLoad = data.region.empty() ? "floor1" : data.region;
    if (!ctx.worldManager->world->LoadRegion(regionToLoad, ctx)) {
        std::cerr << "[PlayerFactory] Failed to load region '" << regionToLoad << "' for " << username << "; the world folder might be missing." << std::endl;
    }

    PositionComponent pos{ 0, 0, 0 };
    int roomToUse = data.room_id > 0 ? data.room_id : 1;
    if (!ctx.worldManager->PutPlayerInRoom(roomToUse, pos)) {
        std::cerr << "[PlayerFactory] Unable to place " << username << " in room " << roomToUse << "; defaulting to room 1." << std::endl;
        if (!ctx.worldManager->PutPlayerInRoom(1, pos)) {
            std::cerr << "[PlayerFactory] Still could not place " << username << "; dropping at origin." << std::endl;
        }
    }
    ctx.registry->AddComponent(player, pos);
    ctx.registry->AddComponent(player, VisualComponent{ "@", "&r" });
    ctx.registry->AddComponent(player, InventoryComponent{});
    ctx.registry->AddComponent(player, EquipmentComponent{});
    
    // Initialize skills system for the player
    SkillHolderComponent skillHolder;
    // Add basic unarmed attack skill - every player should be able to punch
    int punchSkillID = ctx.factories->skills.GetSkillID("skill_punch");
    if (punchSkillID != -1) {
        skillHolder.skillAliases["attack"] = punchSkillID;
        skillHolder.skillAliases["punch"] = punchSkillID;
        skillHolder.mastery[punchSkillID] = 1; // Start with mastery level 1
    }
    ctx.registry->AddComponent(player, skillHolder);

    // 5. Hydrate Inventory
    auto* inv = ctx.registry->GetComponent<InventoryComponent>(player);
    auto* equip = ctx.registry->GetComponent<EquipmentComponent>(player);
    for (auto& itemData : data.items) {
        EntityID item = ctx.factories->items.CreateItem(itemData.templateId);
        // Apply saved state (equipped, durability, etc)
        if (itemData.state.value("equipped", false)) {
            EquipmentSlot slot = (EquipmentSlot)itemData.state["slot"];
            equip->slots[slot] = item;
        }
        else {
            inv->items.push_back(item);
        }
    }

    return player;
}