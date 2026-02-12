#include "MobFactory.h"
#include "GameContext.h"
#include "ScriptManager.h"
#include "Component.h"
#include "TextHelperFunctions.h"
#include "EquipmentSlot.h"
#include <sol/sol.hpp>


void MobFactory::LoadMobTemplatesFromLua() {
    try {
        ctx.scripts->lua.script_file("scripts/data/mobs_master.lua");
        sol::table mobsTable = ctx.scripts->lua["Mobs"];

        if (!mobsTable.valid()) return;

        std::cout << "Loading Mob Templates..." << std::endl;
        for (auto& kv : mobsTable) {
            std::string key = kv.first.as<std::string>();
            sol::table data = kv.second;
            LoadSingleMobFromLua(key, data);
        }
    }
    catch (const sol::error& e) {
        std::cerr << "Lua Error in mobs_master: " << e.what() << std::endl;
    }
}

void MobFactory::LoadSingleMobFromLua(const std::string& key, sol::table& t) {
    MobTemplate tpl;

    tpl.name = t.get_or<std::string>("name", "Unknown Mob");
    tpl.description = t.get_or<std::string>("desc", "A generic creature.");
    tpl.symbol = t.get_or<std::string>("symbol", "m");
    tpl.color = t.get_or<std::string>("color", "&r");
    tpl.hp = t.get_or("hp", 10);
    tpl.level = t.get_or("level", 1);
    tpl.aiType = t.get_or<std::string>("ai", "aggressive"); // Default AI
    tpl.lootTable = t.get_or<std::string>("loot", "");

    // Stats are usually nested in Lua: stats = { str=10, ... }
    sol::table stats = t["stats"];
    if (stats.valid()) {
        tpl.str = stats.get_or("str", 10);
        tpl.dex = stats.get_or("dex", 10);
        tpl.intel = stats.get_or("int", 10);
    }
    else {
        tpl.str = 10; tpl.dex = 10; tpl.intel = 10;
    }

    mobTemplates[key] = tpl;
}

int MobFactory::CreateMob(std::string templateID, json overrides, int x, int y, int roomID) {
    if (mobTemplates.find(templateID) == mobTemplates.end()) {
        std::cerr << "Mob Template not found: " << templateID << std::endl;
        return -1;
    }

    const auto& tpl = mobTemplates[templateID];
    int id = ctx.registry->CreateEntity();

    // 1. Calculate final values (Template + Override)
    // We check if "overrides" has a value, otherwise use template
    std::string name = overrides.value("name", tpl.name);
    int hp = overrides.value("hp", tpl.hp);

    // Handle nested override for stats if exists
    int str = tpl.str;
    int dex = tpl.dex;
    int intel = tpl.intel;

    if (overrides.contains("stats")) {
        auto s = overrides["stats"];
        str = s.value("str", str);
        dex = s.value("dex", dex);
        intel = s.value("int", intel);
    }

    // 2. Add Components
    ctx.registry->AddComponent<NameComponent>(id, { name });
    ctx.registry->AddComponent<DescriptionComponent>(id, { tpl.description }); // overrideable if you want
    ctx.registry->AddComponent<VisualComponent>(id, { tpl.symbol, tpl.color });

    // Health: Current = Max
    ctx.registry->AddComponent<HealthComponent>(id, { hp, hp });

    // Stats
    BaseStatsComponent bsc;
    bsc.strength = str;
    bsc.dexterity = dex;
    bsc.intelligence = intel;
    ctx.registry->AddComponent<BaseStatsComponent>(id, bsc);

    // AI & Tags
    ctx.registry->AddComponent<MobComponent>(id, { tpl.aiType });

    // Loot
    if (!tpl.lootTable.empty()) {
        ctx.registry->AddComponent<LootDropComponent>(id, { tpl.lootTable });
    }

    // Position
    if (roomID != -1) {
        ctx.registry->AddComponent<PositionComponent>(id, { x, y, roomID });
    }

    return id;
}