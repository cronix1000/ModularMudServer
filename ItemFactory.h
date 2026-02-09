#pragma once
#include "GameContext.h"
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include "ItemComponent.h"
#include "WeaponComponent.h"
#include "ArmourComponent.h"
#include "VisualComponent.h"
#include "PositionComponent.h"
#include "NameComponent.h"
#include "DescriptionComponent.h"
#include "TextHelperFunctions.h"
#include "ScriptComponent.h"

using json = nlohmann::json;

// --- The Template (Static Blueprint) ---
struct ItemTemplate {
    std::string name;
    std::string symbol;
    std::string color;
    std::string description;
    int weight;
    int value;
    bool equippable;
    bool gettable;
    json extra; // Raw data for specific components
};

class ItemFactory {
public:
    GameContext& ctx;
    std::map<std::string, ItemTemplate> itemTemplates;

    ItemFactory(GameContext& g) : ctx(g) {}

    void LoadItemTemplates(const std::string& jsonPath) {
        std::ifstream file(jsonPath);
        if (!file.is_open()) return;

        json itemData;
        file >> itemData;

        for (auto& [key, val] : itemData.items()) {
            ItemTemplate t;
            t.name = val.value("name", "unknown");
            t.symbol = val.value("char", "?");
            t.color = val.value("color", "&w");
            t.description = val.value("description", "");
            t.weight = val.value("weight", 1);
            t.value = val.value("value", 0);
            t.equippable = val.value("equippable", false);
            t.gettable = val.value("gettable", true);
            t.extra = val.value("components", json::object());

            itemTemplates[key] = t;
        }
    }

    int CreateItem(std::string templateID, json overrides = "", int x = 0, int y = 0, int roomId = -1, int ownership = -1) {
        auto it = itemTemplates.find(templateID);
        if (it == itemTemplates.end()) return -1;

        const auto& tpl = it->second;
        int id = ctx.registry->CreateEntity();

        // 1. Basic Components (Copying data from template)
        ctx.registry->AddComponent<NameComponent>(id, NameComponent(tpl.name));
        ctx.registry->AddComponent<DescriptionComponent>(id, DescriptionComponent{ tpl.description });
        ctx.registry->AddComponent<VisualComponent>(id, VisualComponent{ tpl.symbol, tpl.color });

        // Item Component stores the "Template Link" + instance data
        ctx.registry->AddComponent<ItemComponent>(id, ItemComponent{
            templateID,
            tpl.weight,
            tpl.value,
            tpl.gettable,
            tpl.equippable
            });
        if (tpl.extra.contains("weapon")) {
            auto w = tpl.extra["weapon"];
            WeaponComponent weapon;
            weapon.templateID = templateID;

            weapon.minDamage = w.value("minDamage", 1);
            weapon.maxDamage = w.value("maxDamage", 1);
            weapon.strScaling = w.value("strScaling", 0.0f);
            weapon.dexScaling = w.value("dexScaling", 0.0f);
            weapon.intScaling = w.value("intScaling", 0.0f);

            // KEEP: Damage Type (e.g., "blunt", "fire") passed to Lua
            weapon.damageType = w.value("damageType", "blunt");

            // NEW: The Skill Alias Link
            weapon.defaultSkillTemplate = w.value("defaultSkill", "skill_punch");

            ctx.registry->AddComponent<WeaponComponent>(id, weapon);
        }

        // 3. Armour Component
        if (tpl.extra.contains("armour")) {
            auto a = tpl.extra["armour"];
            ArmourComponent armour;
            armour.templateID = templateID;

            armour.defense = a.value("defense", 0);
            armour.magicDefense = a.value("magic_defense", 0); // Watch for snake_case in JSON
            armour.slot = TextHelperFunctions::StringToSlot(a.value("slot", "torso"));

            ctx.registry->AddComponent<ArmourComponent>(id, armour);
        }

        // 4. Item Scripts (Procs/Triggers) (UPDATED)
        // Checks for "scripts": { "on_hit": "flame_burst" }
        if (tpl.extra.contains("scripts")) {
            ScriptComponent scripts;
            for (auto& [trigger, file] : tpl.extra["scripts"].items()) {
                scripts.scripts_path[trigger] = file;
            }
            ctx.registry->AddComponent<ScriptComponent>(id, scripts);
            
        }


        if (roomId != -1) {
            ctx.registry->AddComponent<PositionComponent>(id, PositionComponent{ x, y, roomId });
        }

        return id;
    }
};