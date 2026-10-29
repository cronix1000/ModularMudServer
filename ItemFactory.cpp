#include "ItemFactory.h"
#include "SafeJson.h"
#include "GameContext.h"
#include "Component.h"
#include "TextHelperFunctions.h"
#include "EquipmentSlot.h"
#include "EquipmentSlotInfo.h"
#include "SkillFactory.h"
#include "FactoryManager.h"
#include "MetaComponent.h"
#include "MetaRegistry.h"
#include "InteractionOverrideComponent.h"
#include "ScriptComponent.h"
#include <fstream>

void ItemFactory::LoadItemTemplatesFromJSON(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[Error] Could not open items.json: " << path << std::endl;
        return;
    }

    json data;
    try {
        file >> data;
    }
    catch (const json::parse_error& e) {
        std::cerr << "[Error] Failed to parse items.json: " << e.what() << std::endl;
        return;
    }

    LoadItemTemplatesFromJson(data);
}

void ItemFactory::LoadItemTemplatesFromJson(const json& data) {
    std::cout << "Loading Item Templates from JSON..." << std::endl;
    int count = 0;
    for (auto& [key, j] : data.items()) {
        LoadSingleItemFromJSON(key, j);
        count++;
    }
    std::cout << "Loaded " << count << " items." << std::endl;
}

void ItemFactory::LoadSingleItemFromJSON(const std::string& key, const json& j) {
    ItemTemplate tpl;
    tpl.id = key;

    tpl.name = SafeValue(j, "name", "Unknown Item");
    tpl.description = SafeValue(j, "description", "...");
    tpl.symbol = SafeValue(j, "char", "(");
    tpl.color = SafeValue(j, "color", "&w");
    tpl.weight = SafeValue(j, "weight", 1);
    tpl.value = SafeValue(j, "value", 0);
    tpl.equippable = SafeValue(j, "equippable", false);
    tpl.itemType = SafeValue(j, "type", "misc");

    if (j.contains("components")) {
        const json& comps = j["components"];

        if (comps.contains("weapon")) {
            const json& weapon = comps["weapon"];
            tpl.itemType = "weapon";
            tpl.minDamage = SafeValue(weapon, "minDamage", 1);
            tpl.maxDamage = SafeValue(weapon, "maxDamage", 2);
            tpl.damageType = SafeValue(weapon, "damageType", "blunt");
            tpl.primarySkill = SafeValue(weapon, "defaultSkill", "");
            tpl.twoHanded = SafeValue(weapon, "twoHanded", false);
        }

        if (comps.contains("armour")) {
            const json& armor = comps["armour"];
            tpl.itemType = "armour";
            tpl.defense = SafeValue(armor, "defense", 0);
        }

        if (comps.contains("scripts")) {
            const json& scripts = comps["scripts"];
            tpl.script = SafeValue(scripts, "on_hit_proc", "");
            if (scripts.contains("on_equip") && scripts["on_equip"].is_string()) {
                tpl.itemScripts["on_equip"] = scripts["on_equip"].get<std::string>();
            }
            if (scripts.contains("on_unequip") && scripts["on_unequip"].is_string()) {
                tpl.itemScripts["on_unequip"] = scripts["on_unequip"].get<std::string>();
            }
            if (scripts.contains("on_drop") && scripts["on_drop"].is_string()) {
                tpl.itemScripts["on_drop"] = scripts["on_drop"].get<std::string>();
            }
        }

        if (comps.contains("allowed_slots") && comps["allowed_slots"].is_array()) {
            for (const auto& s : comps["allowed_slots"]) {
                if (s.is_string()) {
                    EquipmentSlot parsed = EquipmentSlotInfo::ParseSlotString(s.get<std::string>());
                    if (parsed != EquipmentSlot::Default) {
                        tpl.allowedSlots.push_back(parsed);
                    }
                }
            }
        }

        if (tpl.itemType == "armour" && tpl.allowedSlots.empty()) {
            std::cerr << "[Warning] Item '" << key << "' has armour components with no allowed_slots. "
                      << "Defaulting to [Chest]. Add 'components.allowed_slots' to items.json." << std::endl;
            tpl.allowedSlots.push_back(EquipmentSlot::Chest);
        }
    }

    if (j.contains("components")) {
        const json& comps = j["components"];
        if (comps.contains("passive_skills")) {
            for (const auto& skill : comps["passive_skills"]) {
                tpl.extraSkills.push_back(skill.get<std::string>());
            }
        }
        if (comps.contains("messages") && comps["messages"].is_object()) {
            tpl.messages = comps["messages"];
        }
    }

    tpl.extra = SafeValue(j, "extra", json::object());
    if (j.contains("components") && j["components"].is_object() && j["components"].contains("meta")) {
        tpl.extra["meta"] = j["components"]["meta"];
    }

    itemTemplates[key] = tpl;
}

int ItemFactory::CreateItem(std::string templateID, json overrides, int x, int y, int roomID) {
    if (itemTemplates.find(templateID) == itemTemplates.end()) {
        std::cerr << "Error: Item template '" << templateID << "' not found." << std::endl;
        return -1;
    }

    const auto& tpl = itemTemplates[templateID];
    int id = ctx.registry->CreateEntity();

    std::string name = SafeValue(overrides, "name", tpl.name);
    std::string desc = SafeValue(overrides, "description", tpl.description);

    ctx.registry->AddComponent<NameComponent>(id, { name });
    ctx.registry->AddComponent<DescriptionComponent>(id, { desc });
    ctx.registry->AddComponent<WeightComponent>(id, { tpl.weight });
    ctx.registry->AddComponent<ValueComponent>(id, { tpl.value });
    ctx.registry->AddComponent<VisualComponent>(id, { tpl.symbol, tpl.color });
    ItemComponent itemComp = ItemComponent{ templateID };

    auto addSkillById = [&](const std::string& skillKey, bool primary) {
        int skillId = ctx.factories->skills.GetSkillID(skillKey);
        if (skillId != -1) {
            if (!primary)
                itemComp.extraSkillIds.push_back(skillId);
            else
                itemComp.primarySkillId = skillId;
        }
    };

    if (!tpl.primarySkill.empty()) {
        addSkillById(tpl.primarySkill, true);
    }

    for (const auto& skillName : tpl.extraSkills) {
        addSkillById(skillName, false);
    }

    if (overrides.contains("skills") && overrides["skills"].is_array()) {
        for (const auto& skillName : overrides["skills"]) {
            addSkillById(skillName.get<std::string>(), false);
        }
    }

    itemComp.allowedSlots = tpl.allowedSlots;

    ctx.registry->AddComponent<ItemComponent>(id, itemComp);
    if (roomID != -1) {
        ctx.registry->AddComponent<PositionComponent>(id, { x, y, roomID });
    }

    AttachTypeComponents(id, tpl, overrides);

    if (tpl.extra.is_object() && tpl.extra.contains("meta") && tpl.extra["meta"].is_object()) {
        json meta = tpl.extra["meta"];
        MetaRegistry::ApplyDefaults("item", meta);
        ctx.registry->AddComponent<MetaComponent>(id, MetaComponent{ meta });
    }

    json messagesJson;
    if (overrides.contains("messages") && overrides["messages"].is_object()) {
        messagesJson = overrides["messages"];
    } else if (tpl.messages.is_object()) {
        messagesJson = tpl.messages;
    }
    if (messagesJson.is_object()) {
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

    if (!tpl.itemScripts.empty()) {
        ScriptComponent sc;
        sc.scripts_path = tpl.itemScripts;
        ctx.registry->AddComponent<ScriptComponent>(id, sc);
    }

    return id;
}

void ItemFactory::AttachTypeComponents(int id, const ItemTemplate& tpl, const json& overrides) {
    if (tpl.itemType == "weapon") {
        WeaponComponent wc;
        wc.minDamage = SafeValue(overrides, "minDamage", tpl.minDamage);
        wc.maxDamage = SafeValue(overrides, "maxDamage", tpl.maxDamage);
        wc.damageType = SafeValue(overrides, "damageType", tpl.damageType);
        wc.twoHanded = SafeValue(overrides, "twoHanded", tpl.twoHanded);
        ctx.registry->AddComponent<WeaponComponent>(id, wc);
    }
    else if (tpl.itemType == "armour") {
        ArmourComponent ac;
        ac.defense = SafeValue(overrides, "defense", tpl.defense);
        ctx.registry->AddComponent<ArmourComponent>(id, ac);
    }
}