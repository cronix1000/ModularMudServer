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
#include "PermissionComponent.h"
#include "PlayerVariablesComponent.h"
#include "EventBus.h"
#include "RegionComponent.h"
#include "CommandTrie.h"

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
    auto& s = data.data;
    StatComponent stats;

    auto statsJson = s["stats"];

    // Extract all stat values with defaults
    stats.Health = statsJson.value("hp", 100);
    stats.MaxHealth = statsJson.value("max_hp", 100);
    stats.Strength = statsJson.value("str", 10);
    stats.Dexterity = statsJson.value("dex", 10);
    stats.Intelligence = statsJson.value("int", 10);
    stats.Wisdom = statsJson.value("wis", 10);
    stats.AttackDamage = statsJson.value("atk", 5);
    stats.attackSpeed = statsJson.value("atkspd", 1.0f);
    stats.Mana = statsJson.value("mana", 50);
    ctx.registry->AddComponent(player, stats);

    // Player variables (addon data) from DB
    PlayerVariablesComponent playerVars;
    if (s.contains("variables")) {
        auto& varsJson = s["variables"];
        if (varsJson.contains("intVars")) {
            for (auto& [key, value] : varsJson["intVars"].items()) {
                playerVars.intVars[key] = value.get<int>();
            }
        }
        if (varsJson.contains("stringVars")) {
            for (auto& [key, value] : varsJson["stringVars"].items()) {
                playerVars.stringVars[key] = value.get<std::string>();
            }
        }
    }
    ctx.registry->AddComponent(player, playerVars);

    // Add Player Permissions
    uint8_t level = static_cast<uint8_t>(data.permission);

    ctx.registry->AddComponent(player, PermissionComponent{ level });

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
    ctx.registry->AddComponent(player, RegionComponent{ data.region });
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

    // send event that player logged in 
    PlayerLoggedInData ectx = { player, username, data.permission };
    EventContext event_data;
    event_data.data = ectx;
    ctx.eventBus->Publish(EventType::PlayerJoined, event_data);

    return player;
}