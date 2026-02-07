#pragma once
#include "GameContext.h"
#include "Registry.h"
#include "ResourceCostComponent.h"
#include "CoolDownDefinitionComponent.h"
#include "NameComponent.h"
#include "SkillDefintionComponent.h" // Assuming you have this for Type/Desc
#include <iostream>
#include <map>
#include <string>
#include <sol/sol.hpp>
#include "ScriptManager.h"


class SkillFactory {
public:
    GameContext& ctx;

    // Lookup Map: "heavy_slam" -> EntityID 402
    std::map<std::string, int> skillLookup;

    SkillFactory(GameContext& g) : ctx(g) {}

    void LoadSkillsFromLua() {
        // 1. Ensure the Lua definition file is run
        // This populates the global 'Skills' table in the Lua state
        ctx.scripts.lua.script_file("scripts/skills/skills_master.lua");

        // 2. Get the Global Table
        sol::table skillsTable = ctx.scripts.lua["skills"];
        if (!skillsTable.valid()) {
            std::cerr << "[Error] Global 'Skills' table not found in Lua state!" << std::endl;
            return;
        }

        int count = 0;

        // 3. Iterate over every skill defined in Lua
        for (auto& kv : skillsTable) {
            // key = "slash", "fireball"
            std::string key = kv.first.as<std::string>();
            // value = The table containing {name="...", cooldown=...}
            sol::table skillData = kv.second;

            CreateSkillEntity(key, skillData);
            count++;
        }

        std::cout << "[SkillFactory] Loaded " << count << " skills." << std::endl;
    }

    int GetSkillID(const std::string& key) {
        if (skillLookup.find(key) != skillLookup.end()) {
            return skillLookup[key];
        }
        return -1; // Not found
    }

private:
    void CreateSkillEntity(const std::string& key, const sol::table& skillData) {
        // 1. Create the Entity
        int id = ctx.registry.CreateEntity();

        // 2. Add Identity (Name, Description, Type)
        std::string name = skillData.get_or<std::string>("name", "Unknown Skill");
        std::string type = skillData.get_or<std::string>("type", "generic");
        // Check for description in top level, or inside 'data' if you prefer
        std::string desc = skillData.get_or<std::string>("description", "");

        ctx.registry.AddComponent<NameComponent>(id, { name });
        // Assuming you have a component to store static skill info
        // ctx.registry.AddComponent<SkillDefinitionComponent>(id, { name, type, desc });

        // 3. Add Script Link (CRITICAL)
        // We map "on_use" -> the key ("slash"). 
        // When SkillSystem runs, it looks up Skills["slash"] in Lua.
        ScriptComponent sc;
        sc.scripts_path["on_use"] = key;
        ctx.registry.AddComponent<ScriptComponent>(id, sc);

        // 4. Add Costs
        // Checks for: costs = { stamina = 10, mana = 5 }
        sol::optional<sol::table> costsOpt = skillData["costs"];
        if (costsOpt && costsOpt->valid()) {
            sol::table costs = costsOpt.value();
            ResourceCostComponent rc;

            rc.stamina = costs.get_or("stamina", 0);
            rc.mana = costs.get_or("mana", 0);
            rc.health = costs.get_or("health", 0);

            if (rc.stamina > 0 || rc.mana > 0 || rc.health > 0) {
                ctx.registry.AddComponent<ResourceCostComponent>(id, rc);
            }
        }

        // 5. Add Cooldowns / Timing
        // Checks for: cooldown = 2.5 (float) OR cooldown = { time=2.5 }
        // Based on previous prompts, it's likely a flat float at the top level
        float cdTime = skillData.get_or("cooldown", 0.0f);
        float wuTime = skillData.get_or("windup", 0.0f);

        // Also check inside 'data' just in case
        sol::optional<sol::table> dataOpt = skillData["data"];
        if (dataOpt && dataOpt->valid()) {
            sol::table dataTbl = dataOpt.value();
            if (wuTime == 0.0f) wuTime = dataTbl.get_or("windup", 0.0f);
        }

        if (cdTime > 0 || wuTime > 0) {
            CooldownStatsComponent cc;
            cc.cooldownTime = cdTime;
            cc.windupTime = wuTime;
            ctx.registry.AddComponent<CooldownStatsComponent>(id, cc);
        }

        // 6. Register in Lookup Map
        skillLookup[key] = id;
    }
};