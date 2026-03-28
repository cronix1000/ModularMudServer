#include "ItemFactory.h"
#include "GameContext.h"
#include "Component.h"
#include "TextHelperFunctions.h"
#include "EquipmentSlot.h"
#include "SkillFactory.h"
#include "FactoryManager.h"
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

    tpl.name = j.value("name", "Unknown Item");
    tpl.description = j.value("description", "...");
    tpl.symbol = j.value("char", "(");
    tpl.color = j.value("color", "&w");
    tpl.weight = j.value("weight", 1);
    tpl.value = j.value("value", 0);
    tpl.equippable = j.value("equippable", false);
    tpl.itemType = j.value("type", "misc");

    if (j.contains("components")) {
        const json& comps = j["components"];

        if (comps.contains("weapon")) {
            const json& weapon = comps["weapon"];
            tpl.itemType = "weapon";
            tpl.minDamage = weapon.value("minDamage", 1);
            tpl.maxDamage = weapon.value("maxDamage", 2);
            tpl.damageType = weapon.value("damageType", "blunt");
            tpl.primarySkill = weapon.value("defaultSkill", "");
        }

        if (comps.contains("armour")) {
            const json& armor = comps["armour"];
            tpl.itemType = "armour";
            tpl.defense = armor.value("defense", 0);
            tpl.slot = armor.value("slot", "torso");
        }

        if (comps.contains("scripts")) {
            const json& scripts = comps["scripts"];
            tpl.script = scripts.value("on_hit_proc", "");
        }
    }

    if (j.contains("components")) {
        const json& comps = j["components"];
        if (comps.contains("passive_skills")) {
            for (const auto& skill : comps["passive_skills"]) {
                tpl.extraSkills.push_back(skill.get<std::string>());
            }
        }
    }

    tpl.extra = j.value("extra", json::object());

    itemTemplates[key] = tpl;
}

int ItemFactory::CreateItem(std::string templateID, json overrides, int x, int y, int roomID) {
    if (itemTemplates.find(templateID) == itemTemplates.end()) {
        std::cerr << "Error: Item template '" << templateID << "' not found." << std::endl;
        return -1;
    }

    const auto& tpl = itemTemplates[templateID];
    int id = ctx.registry->CreateEntity();

    std::string name = overrides.value("name", tpl.name);
    std::string desc = overrides.value("description", tpl.description);

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

    ctx.registry->AddComponent<ItemComponent>(id, itemComp);
    if (roomID != -1) {
        ctx.registry->AddComponent<PositionComponent>(id, { x, y, roomID });
    }

    AttachTypeComponents(id, tpl, overrides);

    return id;
}

void ItemFactory::AttachTypeComponents(int id, const ItemTemplate& tpl, const json& overrides) {
    if (tpl.itemType == "weapon") {
        WeaponComponent wc;
        wc.minDamage = overrides.value("minDamage", tpl.minDamage);
        wc.maxDamage = overrides.value("maxDamage", tpl.maxDamage);
        wc.damageType = overrides.value("damageType", tpl.damageType);
        ctx.registry->AddComponent<WeaponComponent>(id, wc);
    }
    else if (tpl.itemType == "armour") {
        ArmourComponent ac;
        ac.defense = overrides.value("defense", tpl.defense);
        std::string slot = overrides.value("slot", tpl.slot);
        EquipmentSlot eqSlot = TextHelperFunctions::StringToSlot(slot);
        ac.slot = eqSlot;
        ctx.registry->AddComponent<ArmourComponent>(id, ac);
    }
}