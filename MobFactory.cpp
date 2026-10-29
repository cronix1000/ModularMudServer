#include "MobFactory.h"
#include "SafeJson.h"
#include "GameContext.h"
#include "Component.h"
#include "TextHelperFunctions.h"
#include "MobAttackComponent.h"
#include "ScriptManager.h"
#include "MetaRegistry.h"
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

    LoadMobTemplatesFromJson(data);
}

void MobFactory::LoadMobTemplatesFromJson(const json& data) {
    std::cout << "Loading Mob Templates from JSON..." << std::endl;
    for (auto& [key, j] : data.items()) {
        LoadSingleMobFromJSON(key, j);
    }
}

void MobFactory::LoadSingleMobFromJSON(const std::string& key, const json& j) {
    MobTemplate tpl;
    tpl.id = key;

    tpl.name = SafeValue(j, "name", "Unknown Mob");
    tpl.description = SafeValue(j, "description", "A generic creature.");
    tpl.symbol = SafeValue(j, "char", "m");
    tpl.color = SafeValue(j, "color", "&r");
    tpl.hp = SafeValue(j, "hp", 10);
    tpl.level = SafeValue(j, "level", 1);
    tpl.aiType = SafeValue(j, "ai", "aggressive");
    tpl.lootTable = SafeValue(j, "loot_drop", "");

    if (j.contains("stat")) {
        const json& stats = j["stat"];
        tpl.str = SafeValue(stats, "strength", 10);
        tpl.dex = SafeValue(stats, "dexterity", 10);
        tpl.intel = SafeValue(stats, "intelligence", 10);
    }
    else {
        tpl.str = 10; tpl.dex = 10; tpl.intel = 10;
    }

    tpl.attackDamage = SafeValue(j, "attack_damage", 5);
    tpl.attackSpeed = SafeValue(j, "attack_speed", 2.0f);
    tpl.criticalChance = SafeValue(j, "critical_chance", 0.05f);
    tpl.criticalMultiplier = SafeValue(j, "critical_multiplier", 1.5f);

    if (j.contains("attack_patterns")) {
        for (const auto& pattern : j["attack_patterns"]) {
            MobAttackPattern mobPattern;
            mobPattern.verb = SafeValue(pattern, "verb", "attacks");
            mobPattern.multiplier = SafeValue(pattern, "multiplier", 1.0f);
            mobPattern.damageType = SafeValue(pattern, "damage_type", "physical");
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
        if (comps.contains("meta")) {
            tpl.extra["meta"] = comps["meta"];
        }
        if (comps.contains("messages") && comps["messages"].is_object()) {
            tpl.extra["messages"] = comps["messages"];
        }
    }
    if (j.contains("messages") && j["messages"].is_object()) {
        tpl.extra["messages"] = j["messages"];
    }

    tpl.script = SafeValue(j, "script", "");
    json extraDoc = SafeValue(j, "extra", json::object());
    for (auto extraIt = extraDoc.begin(); extraIt != extraDoc.end(); ++extraIt) {
        tpl.extra[extraIt.key()] = extraIt.value();
    }

    mobTemplates[key] = tpl;
}
/*
* TODO Attach Respawn Component 
**/
int MobFactory::CreateMob(std::string templateID, json overrides, int x, int y, int roomID) {
    if (mobTemplates.find(templateID) == mobTemplates.end()) {
        std::cerr << "Mob Template not found: " << templateID << std::endl;
        return -1;
    }

    const auto& tpl = mobTemplates[templateID];
    int id = ctx.registry->CreateEntity();

    std::string name = SafeValue(overrides, "name", tpl.name);
    int hp = SafeValue(overrides, "hp", tpl.hp);

    int str = tpl.str;
    int dex = tpl.dex;
    int intel = tpl.intel;

    if (overrides.contains("stats")) {
        auto s = overrides["stats"];
        str = SafeValue(s, "str", str);
        dex = SafeValue(s, "dex", dex);
        intel = SafeValue(s, "int", intel);
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

    if (!tpl.script.empty() && ctx.scripts) {
        sol::protected_function func = ctx.scripts->lua["mobs"][templateID]["on_attack"];
        if (func.valid()) {
            mobAttack.luaAttackFunc = func;
            mobAttack.hasLuaHook = true;
        } else {
            std::cerr << "[MobFactory] Mob '" << templateID
                      << "' declares script_ref '" << tpl.script
                      << "' but no 'on_attack' function found in mobs[\""
                      << templateID << "\"]." << std::endl;
        }
    }

    ctx.registry->AddComponent<MobAttackComponent>(id, mobAttack);

    ctx.registry->AddComponent<MobComponent>(id, { tpl.aiType });

    BehaviourComponent behaviour;
    if (tpl.aiType == "aggressive") {
        behaviour.behaviourType = BehaviourType::aggressive;
    } else if (tpl.aiType == "passive") {
        behaviour.behaviourType = BehaviourType::passive;
    } else if (tpl.aiType == "neutral") {
        behaviour.behaviourType = BehaviourType::neutral;
    } else if (tpl.aiType == "boss") {
        behaviour.behaviourType = BehaviourType::aggressive;
    } else {
        behaviour.behaviourType = BehaviourType::neutral;
    }
    ctx.registry->AddComponent<BehaviourComponent>(id, behaviour);

    if (!tpl.lootTable.empty()) {
        ctx.registry->AddComponent<LootDropComponent>(id, { tpl.lootTable });
    }

    if (roomID != -1) {
        ctx.registry->AddComponent<PositionComponent>(id, { x, y, roomID });
    }

    json meta = SafeValue(tpl.extra, "meta", json::object());
    if (overrides.contains("meta") && overrides["meta"].is_object()) {
        for (auto& [k, v] : overrides["meta"].items()) {
            meta[k] = v;
        }
    }
    MetaRegistry::ApplyDefaults("mob", meta);
    if (!meta.empty()) {
        ctx.registry->AddComponent<MetaComponent>(id, MetaComponent{ meta });
    }

    json messagesJson;
    if (overrides.contains("messages") && overrides["messages"].is_object()) {
        messagesJson = overrides["messages"];
    } else if (tpl.extra.contains("messages") && tpl.extra["messages"].is_object()) {
        messagesJson = tpl.extra["messages"];
    } else {
        static thread_local json s_empty = json::object();
        messagesJson = s_empty;
    }
    if (messagesJson.is_object() && !messagesJson.empty()) {
        InteractionOverrideComponent ioc;
        for (auto& [k, v] : messagesJson.items()) {
            if (v.is_array()) {
                for (auto& m : v) {
                    if (m.is_string()) ioc.overrides[k].push_back(m.get<std::string>());
                }
            }
        }
        if (!ioc.overrides.empty()) {
            ctx.registry->AddComponent<InteractionOverrideComponent>(id, ioc);
        }
    }

    return id;
}