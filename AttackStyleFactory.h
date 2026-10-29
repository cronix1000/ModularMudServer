#pragma once
#include "GameContext.h"
#include "SafeJson.h"
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
#include "EquipmentSlotInfo.h"
#include "InteractionOverrideComponent.h"
#include "Registry.h"

using json = nlohmann::json;

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
            t.name = SafeValue(val, "name", "unknown");
            t.symbol = SafeValue(val, "char", "?");
            t.color = SafeValue(val, "color", "&w");
            t.description = SafeValue(val, "description", "");
            t.weight = SafeValue(val, "weight", 1);
            t.value = SafeValue(val, "value", 0);
            t.equippable = SafeValue(val, "equippable", false);
            t.gettable = SafeValue(val, "gettable", true);
            t.extra = SafeValue(val, "components", json::object());

            attackStyleTemplates[key] = t;
        }
    }

    int CreateItem(std::string templateID, json overrides = "", int x = 0, int y = 0, int roomId = -1, int ownership = -1) {
        auto it = attackStyleTemplates.find(templateID);
        if (it == attackStyleTemplates.end()) return -1;

        const auto& tpl = it->second;
        int id = ctx.registry->CreateEntity();

        // 1. Basic Components (Copying data from template)
        ctx.registry->AddComponent<NameComponent>(id, NameComponent(tpl.name));
        ctx.registry->AddComponent<DescriptionComponent>(id, DescriptionComponent{ tpl.description });
        ctx.registry->AddComponent<VisualComponent>(id, VisualComponent{ tpl.symbol, tpl.color });

        // Item Component stores the "Template Link" + instance data
        ItemComponent itemCmp;
        itemCmp.templateName = templateID;
        itemCmp.allowedSlots.clear();
        ctx.registry->AddComponent<ItemComponent>(id, itemCmp);

        // 2. INJECTION: Check "extra" JSON and push data into components
        // This is where we avoid future lookups.

        if (tpl.extra.contains("weapon")) {
            auto w = tpl.extra["weapon"];
            WeaponComponent weapon;
            weapon.templateID = templateID;

            // WE COPY THE NUMBERS HERE
            weapon.minDamage = SafeValue(w, "minDamage", 1);
            weapon.maxDamage = SafeValue(w, "maxDamage", 1);
            weapon.strScaling = SafeValue(w, "strScaling", 0.0f);
            weapon.dexScaling = SafeValue(w, "dexScaling", 0.0f);
            weapon.intScaling = SafeValue(w, "intScaling", 0.0f);
            weapon.damageType = SafeValue(w, "damageType", "blunt");
            weapon.twoHanded = SafeValue(w, "twoHanded", false);

            ctx.registry->AddComponent<WeaponComponent>(id, weapon);
        }

        if (tpl.extra.contains("armour")) {
            auto a = tpl.extra["armour"];
            ArmourComponent armour;
            armour.templateID = templateID;

            // WE COPY THE NUMBERS HERE
            armour.defense = SafeValue(a, "defense", 0);
            armour.magicDefense = SafeValue(a, "magicDefense", 0);

            ctx.registry->AddComponent<ArmourComponent>(id, armour);
        }

        // Item-level scripts (on_equip / on_unequip / on_drop)
        if (tpl.extra.contains("scripts") && tpl.extra["scripts"].is_object()) {
            const json& scripts = tpl.extra["scripts"];
            bool hasAny = false;
            ScriptComponent sc;
            if (scripts.contains("on_equip") && scripts["on_equip"].is_string()) {
                sc.scripts_path["on_equip"] = scripts["on_equip"].get<std::string>();
                hasAny = true;
            }
            if (scripts.contains("on_unequip") && scripts["on_unequip"].is_string()) {
                sc.scripts_path["on_unequip"] = scripts["on_unequip"].get<std::string>();
                hasAny = true;
            }
            if (scripts.contains("on_drop") && scripts["on_drop"].is_string()) {
                sc.scripts_path["on_drop"] = scripts["on_drop"].get<std::string>();
                hasAny = true;
            }
            if (hasAny) {
                ctx.registry->AddComponent<ScriptComponent>(id, sc);
            }
        }

        // 3. Position logic
        if (roomId != -1) {
            ctx.registry->AddComponent<PositionComponent>(id, PositionComponent{ x, y, roomId });
        }

        // 4. Interaction override messages from tpl.extra.messages
        if (tpl.extra.contains("messages") && tpl.extra["messages"].is_object()) {
            const json& messagesJson = tpl.extra["messages"];
            InteractionOverrideComponent ioc;
            for (auto it = messagesJson.begin(); it != messagesJson.end(); ++it) {
                if (it.value().is_array()) {
                    for (auto& m : it.value()) {
                        if (m.is_string()) ioc.overrides[it.key()].push_back(m.get<std::string>());
                    }
                }
            }
            if (!ioc.overrides.empty()) {
                ctx.registry->AddComponent<InteractionOverrideComponent>(id, ioc);
            }
        }

        return id;
    }
};