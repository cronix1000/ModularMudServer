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
#include "Registry.h"


using json = nlohmann::json;

// --- The Template (Static Blueprint) ---
struct AttackStyleTemplate {
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

class AttackStyleFactory {
public:
    GameContext& ctx;
    std::map<std::string, AttackStyleTemplate> attackStyleTemplates;

    AttackStyleFactory(GameContext& g) : ctx(g) {}

    void LoadItemTemplates(const std::string& jsonPath) {
        std::ifstream file(jsonPath);
        if (!file.is_open()) return;

        json itemData;
        file >> itemData;

        for (auto& [key, val] : itemData.items()) {
            AttackStyleTemplate t;
            t.name = val.value("name", "unknown");
            t.symbol = val.value("char", "?");
            t.color = val.value("color", "&w");
            t.description = val.value("description", "");
            t.weight = val.value("weight", 1);
            t.value = val.value("value", 0);
            t.equippable = val.value("equippable", false);
            t.gettable = val.value("gettable", true);
            t.extra = val.value("components", json::object());

            attackStyleTemplates[key] = t;
        }
    }

    int CreateItem(std::string templateID, json overrides = "", int x = 0, int y = 0, int roomId = -1, int ownership = -1) {
        auto it = attackStyleTemplates.find(templateID);
        if (it == attackStyleTemplates.end()) return -1;

        const auto& tpl = it->second;
        int id = ctx.registry.CreateEntity();

        // 1. Basic Components (Copying data from template)
        ctx.registry.AddComponent<NameComponent>(id, NameComponent(tpl.name));
        ctx.registry.AddComponent<DescriptionComponent>(id, DescriptionComponent{ tpl.description });
        ctx.registry.AddComponent<VisualComponent>(id, VisualComponent{ tpl.symbol, tpl.color });

        // Item Component stores the "Template Link" + instance data
        ctx.registry.AddComponent<ItemComponent>(id, ItemComponent{
            templateID,
            tpl.weight,
            tpl.value,
            tpl.gettable,
            tpl.equippable
            });

        // 2. INJECTION: Check "extra" JSON and push data into components
        // This is where we avoid future lookups.

        if (tpl.extra.contains("weapon")) {
            auto w = tpl.extra["weapon"];
            WeaponComponent weapon;
            weapon.templateID = templateID;

            // WE COPY THE NUMBERS HERE
            weapon.minDamage = w.value("minDamage", 1);
            weapon.maxDamage = w.value("maxDamage", 1);
            weapon.strScaling = w.value("strScaling", 0.0f);
            weapon.dexScaling = w.value("dexScaling", 0.0f);
            weapon.intScaling = w.value("intScaling", 0.0f);
            weapon.damageType = w.value("damageType", "blunt");

            ctx.registry.AddComponent<WeaponComponent>(id, weapon);
        }

        if (tpl.extra.contains("armour")) {
            auto a = tpl.extra["armour"];
            ArmourComponent armour;
            armour.templateID = templateID;

            // WE COPY THE NUMBERS HERE
            armour.defense = a.value("defense", 0);
            armour.magicDefense = a.value("magicDefense", 0);
            armour.slot = TextHelperFunctions::StringToSlot(a.value("slot", "torso"));

            ctx.registry.AddComponent<ArmourComponent>(id, armour);
        }

        // 3. Position logic
        if (roomId != -1) {
            ctx.registry.AddComponent<PositionComponent>(id, PositionComponent{ x, y, roomId });
        }

        return id;
    }
};