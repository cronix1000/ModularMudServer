#include "SkillFactory.h"
#include "GameContext.h"
#include "Component.h"
#include "ScriptComponent.h"
#include "ScriptManager.h"
#include <fstream>

void SkillFactory::LoadSkillsFromJSON(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[Error] Could not open skills.json: " << path << std::endl;
        return;
    }

    json data;
    try {
        file >> data;
    }
    catch (const json::parse_error& e) {
        std::cerr << "[Error] Failed to parse skills.json: " << e.what() << std::endl;
        return;
    }

    std::cout << "Loading Skills from JSON..." << std::endl;

    if (data.contains("skill_categories")) {
        LoadCategoriesFromJSON(data["skill_categories"]);
    }

    int count = 0;
    if (data.contains("skills")) {
        for (auto& [key, skillData] : data["skills"].items()) {
            LoadSingleSkillFromJSON(key, skillData);
            count++;
        }
    }

    std::cout << "[SkillFactory] Loaded " << count << " skills and " << categories.size() << " categories." << std::endl;
}

void SkillFactory::LoadCategoriesFromJSON(const json& data) {
    for (auto& [key, catData] : data.items()) {
        SkillCategory cat;
        cat.id = key;
        cat.name = catData.value("name", key);
        cat.description = catData.value("description", "");
        cat.synergyBonus = catData.value("synergyBonus", 0.1f);

        if (catData.contains("stats")) {
            for (const auto& s : catData["stats"]) {
                cat.stats.push_back(s.get<std::string>());
            }
        }

        categories[key] = cat;
    }
}

void SkillFactory::LoadSingleSkillFromJSON(const std::string& key, const json& skillData) {
    SkillTemplate tpl;
    tpl.id = key;
    tpl.category = skillData.value("category", "");
    tpl.name = skillData.value("name", "Unknown Skill");
    tpl.description = skillData.value("description", "");
    tpl.type = skillData.value("type", "combat");
    tpl.activation = skillData.value("activation", "weapon");
    tpl.command = skillData.value("command", key);
    tpl.cooldown = skillData.value("cooldown", 0.0f);
    tpl.windup = skillData.value("windup", 0.0f);
    tpl.targeting = skillData.value("targeting", "enemy");
    tpl.range = skillData.value("range", 1);
    tpl.script = skillData.value("script", "");

    if (skillData.contains("costs")) {
        const json& costs = skillData["costs"];
        tpl.staminaCost = costs.value("stamina", 0);
        tpl.manaCost = costs.value("mana", 0);
        tpl.healthCost = costs.value("health", 0);
    }

    CreateSkillEntity(tpl);
}

void SkillFactory::CreateSkillEntity(const SkillTemplate& tpl) {
    int id = ctx.registry->CreateEntity();

    ctx.registry->AddComponent<NameComponent>(id, { tpl.name });

    SkillDefinitionComponent sdc;
    sdc.name = tpl.name;
    sdc.description = tpl.description;
    sdc.type = tpl.type;
    ctx.registry->AddComponent<SkillDefinitionComponent>(id, sdc);

    ScriptComponent sc;
    sc.scripts_path["on_use"] = tpl.script;
    ctx.registry->AddComponent<ScriptComponent>(id, sc);

    if (tpl.staminaCost > 0 || tpl.manaCost > 0 || tpl.healthCost > 0) {
        ResourceCostComponent rc;
        rc.stamina = tpl.staminaCost;
        rc.mana = tpl.manaCost;
        rc.health = tpl.healthCost;
        ctx.registry->AddComponent<ResourceCostComponent>(id, rc);
    }

    if (tpl.cooldown > 0 || tpl.windup > 0) {
        CooldownStatsComponent cc;
        cc.cooldownTime = tpl.cooldown;
        cc.windupTime = tpl.windup;
        ctx.registry->AddComponent<CooldownStatsComponent>(id, cc);
    }

    skillLookup[tpl.id] = id;
}