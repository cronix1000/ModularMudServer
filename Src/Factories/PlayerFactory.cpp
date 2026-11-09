#include "PlayerFactory.h"
#include "SafeJson.h"
#include <iostream>
#include "PlayerData.h"
#include "GameContext.h"
#include "Registry.h"
#include "CombatComponents.h"
#include "InventoryComponents.h"
#include "MovementComponents.h"
#include "PlayerComponents.h"
#include "PresentationComponents.h"
#include "StatsComponents.h"
#include "WorldComponents.h"
#include "FactoryManager.h"
#include "PowerCalculator.h"
#include "WorldManager.h"
#include "World.h"
#include "EventBus.h"
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
    ctx.registry->AddComponent(player, NameComponent{ username });

    // Stats from DB
    auto& s = data.data;
    StatComponent stats;

    auto statsJson = s["stats"];

    // Extract all stat values with defaults
    stats.Health = SafeValue(statsJson, "hp", 100);
    stats.MaxHealth = SafeValue(statsJson, "max_hp", 100);
    stats.Strength = SafeValue(statsJson, "str", 10);
    stats.Dexterity = SafeValue(statsJson, "dex", 10);
    stats.Intelligence = SafeValue(statsJson, "int", 10);
    stats.Wisdom = SafeValue(statsJson, "wis", 10);
    stats.AttackDamage = SafeValue(statsJson, "atk", 5);
    stats.attackSpeed = SafeValue(statsJson, "atkspd", 1.0f);
    stats.Mana = SafeValue(statsJson, "mana", 50);
    ctx.registry->AddComponent(player, stats);

    BaseStatComponent baseStats;
    baseStats.Health = stats.Health;
    baseStats.MaxHealth = stats.MaxHealth;
    baseStats.Armour = 0;
    baseStats.MagicDefense = 0;
    baseStats.Mana = stats.Mana;
    baseStats.Strength = stats.Strength;
    baseStats.Intelligence = stats.Intelligence;
    baseStats.Dexterity = stats.Dexterity;
    baseStats.AttackDamage = stats.AttackDamage;
    baseStats.MagicDamage = 0;
    baseStats.damageType = stats.DamageType;
    ctx.registry->AddComponent(player, baseStats);

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

    playerVars.intVars["gold"] = data.gold;
    playerVars.intVars["bank_balance"] = data.bankBalance;
    playerVars.intVars["level"] = data.level;
    if (!data.classId.empty()) playerVars.stringVars["class_id"] = data.classId;
    if (!data.raceId.empty())  playerVars.stringVars["race_id"]  = data.raceId;

    ctx.registry->AddComponent(player, playerVars);
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
    if (s.contains("skills") && s["skills"].is_object()) {
        for (auto& [key, skillJson] : s["skills"].items()) {
            int skillEntity = ctx.factories->skills.GetSkillID(key);
            if (skillEntity == -1) continue;
            SkillProgress progress;
            progress.level = SafeValue(skillJson, "level", 1);
            progress.xp = SafeValue(skillJson, "xp", 0);
            progress.uses = SafeValue(skillJson, "uses", 0);
            skillHolder.skills[skillEntity] = progress;
        }
    }
    ctx.registry->AddComponent(player, skillHolder);

    MonsterKillTrackerComponent killTracker;
    if (s.contains("monster_kills") && s["monster_kills"].is_object()) {
        for (auto& [mobId, count] : s["monster_kills"].items()) {
            if (!count.is_number()) continue;
            killTracker.monsterKillTracker[mobId] = count.get<int>();
        }
    }
    ctx.registry->AddComponent(player, killTracker);

    // 5. Hydrate Inventory
    auto* inv = ctx.registry->GetComponent<InventoryComponent>(player);
    auto* equip = ctx.registry->GetComponent<EquipmentComponent>(player);
    for (auto& itemData : data.items) {
        EntityID item = ctx.factories->items.CreateItem(itemData.templateId);
        // Apply saved state (equipped, durability, etc)
        if (SafeValue(itemData.state, "equipped", false)) {
            EquipmentSlot slot = EquipmentSlot::Default;
            int32_t rawSlot = SafeValue(itemData.state, "slot", static_cast<int32_t>(EquipmentSlot::Default));
            if (rawSlot >= 0 && rawSlot <= static_cast<int32_t>(EquipmentSlot::Default)) {
                slot = static_cast<EquipmentSlot>(rawSlot);
            }
            if (slot == EquipmentSlot::Default && itemData.state.contains("slot_string")) {
                slot = EquipmentSlotInfo::ParseSlotString(itemData.state["slot_string"].get<std::string>());
            }
            if (slot == EquipmentSlot::Default) {
                inv->items.push_back(item);
            } else {
                equip->slots[slot] = item;
            }
        }
        else {
            inv->items.push_back(item);
        }
    }

    // send event that player logged in
    PowerCalculator::RecalculateDerivedStats(ctx.registry.get(), player);

    PlayerLoggedInData ectx = { player, username, data.permission };
    EventContext event_data;
    event_data.data = ectx;
    ctx.eventBus->Publish(EventType::PlayerJoined, event_data);

    return player;
}