#include "PlayerFactory.h"
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
        printf("[Factory] Creating new record for %s\n", username.c_str());
        int newDbId = ctx.db->CreatePlayerRow(username);
        if (newDbId == -1) return -1; // Database error
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

    if(data.region != "")
        ctx.worldManager->world->LoadRegion(data.region, ctx);
    else
        ctx.worldManager->world->LoadRegion("floor1", ctx);

    PositionComponent pos;
    ctx.worldManager->PutPlayerInRoom(data.room_id, pos);
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