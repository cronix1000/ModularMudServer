#include "MobFactory.h"
#include "GameContext.h"
#include "Component.h"
#include "TextHelperFunctions.h"
#include "EquipmentSlot.h"
#include "StatComponent.h"
#include "MobAttackComponent.h"
#include <fstream>

void MobFactory::LoadMobTemplatesFromJSON(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[Error] Could not open mobs.json: " << path << std::endl;
        return;
    }

    json data;
    try {
        file >> data;
    }
    catch (const json::parse_error& e) {
        std::cerr << "[Error] Failed to parse mobs.json: " << e.what() << std::endl;
        return;
    }

    std::cout << "Loading Mob Templates from JSON..." << std::endl;
    for (auto& [key, j] : data.items()) {
        LoadSingleMobFromJSON(key, j);
    }
}

void MobFactory::LoadSingleMobFromJSON(const std::string& key, const json& j) {
    MobTemplate tpl;
    tpl.id = key;

    tpl.name = j.value("name", "Unknown Mob");
    tpl.description = j.value("description", "A generic creature.");
    tpl.symbol = j.value("char", "m");
    tpl.color = j.value("color", "&r");
    tpl.hp = j.value("hp", 10);
    tpl.level = j.value("level", 1);
    tpl.aiType = j.value("ai", "aggressive");
    tpl.lootTable = j.value("loot_drop", "");

    if (j.contains("stat")) {
        const json& stats = j["stat"];
        tpl.str = stats.value("strength", 10);
        tpl.dex = stats.value("dexterity", 10);
        tpl.intel = stats.value("intelligence", 10);
    }
    else {
        tpl.str = 10; tpl.dex = 10; tpl.intel = 10;
    }

    tpl.attackDamage = j.value("attack_damage", 5);
    tpl.attackSpeed = j.value("attack_speed", 2.0f);
    tpl.criticalChance = j.value("critical_chance", 0.05f);
    tpl.criticalMultiplier = j.value("critical_multiplier", 1.5f);

    if (j.contains("attack_patterns")) {
        for (const auto& pattern : j["attack_patterns"]) {
            MobAttackPattern mobPattern;
            mobPattern.verb = pattern.value("verb", "attacks");
            mobPattern.multiplier = pattern.value("multiplier", 1.0f);
            mobPattern.damageType = pattern.value("damage_type", "physical");
            tpl.attackPatterns.push_back(mobPattern);
        }
    }

    if (tpl.attackPatterns.empty()) {
        MobAttackPattern defaultPattern;
        defaultPattern.verb = "attacks";
        defaultPattern.multiplier = 1.0f;
        defaultPattern.damageType = "physical";
        tpl.attackPatterns.push_back(defaultPattern);
    }

    if (j.contains("components")) {
        const json& comps = j["components"];
        if (comps.contains("faction")) {
            tpl.extra["faction"] = comps["faction"];
        }
    }

    tpl.script = j.value("script", "");
    tpl.extra = j.value("extra", json::object());

    mobTemplates[key] = tpl;
}

int MobFactory::CreateMob(std::string templateID, json overrides, int x, int y, int roomID) {
    if (mobTemplates.find(templateID) == mobTemplates.end()) {
        std::cerr << "Mob Template not found: " << templateID << std::endl;
        return -1;
    }

    const auto& tpl = mobTemplates[templateID];
    int id = ctx.registry->CreateEntity();

    std::string name = overrides.value("name", tpl.name);
    int hp = overrides.value("hp", tpl.hp);

    int str = tpl.str;
    int dex = tpl.dex;
    int intel = tpl.intel;

    if (overrides.contains("stats")) {
        auto s = overrides["stats"];
        str = s.value("str", str);
        dex = s.value("dex", dex);
        intel = s.value("int", intel);
    }

    ctx.registry->AddComponent<NameComponent>(id, { name });
    ctx.registry->AddComponent<DescriptionComponent>(id, { tpl.description });
    ctx.registry->AddComponent<VisualComponent>(id, { tpl.symbol, tpl.color });

    ctx.registry->AddComponent<HealthComponent>(id, { hp, hp });

    BaseStatsComponent bsc;
    bsc.strength = str;
    bsc.dexterity = dex;
    bsc.intelligence = intel;
    ctx.registry->AddComponent<BaseStatsComponent>(id, bsc);

    StatComponent statComp;
    statComp.MaxHealth = hp;
    statComp.Health = hp;
    statComp.Strength = str;
    statComp.Dexterity = dex;
    statComp.Intelligence = intel;
    statComp.AttackDamage = tpl.attackDamage;
    statComp.attackSpeed = static_cast<int>(tpl.attackSpeed * 10);
    ctx.registry->AddComponent<StatComponent>(id, statComp);

    MobAttackComponent mobAttack;
    mobAttack.attackDamage = tpl.attackDamage;
    mobAttack.attackSpeed = tpl.attackSpeed;
    mobAttack.criticalChance = tpl.criticalChance;
    mobAttack.criticalMultiplier = tpl.criticalMultiplier;
    mobAttack.attackPatterns = tpl.attackPatterns;
    mobAttack.hasLuaHook = false;
    ctx.registry->AddComponent<MobAttackComponent>(id, mobAttack);

    ctx.registry->AddComponent<MobComponent>(id, { tpl.aiType });

    if (!tpl.lootTable.empty()) {
        ctx.registry->AddComponent<LootDropComponent>(id, { tpl.lootTable });
    }

    if (roomID != -1) {
        ctx.registry->AddComponent<PositionComponent>(id, { x, y, roomID });
    }

    return id;
}