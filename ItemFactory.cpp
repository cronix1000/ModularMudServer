#include "ItemFactory.h"
#include "GameContext.h"
#include "ScriptManager.h"
#include "Component.h"
#include "TextHelperFunctions.h"
#include "EquipmentSlot.h"
void ItemFactory::LoadItemTemplatesFromLua() {
    // 1. Run the script
    try {
        ctx.scripts->lua.script_file("scripts/items/items_master.lua");
        sol::table itemsTable = ctx.scripts->lua["items"];

        if (!itemsTable.valid()) {
            std::cerr << "[Error] Global 'Items' table not found in Lua!" << std::endl;
            return;
        }

        std::cout << "Loading Item Templates..." << std::endl;
        int count = 0;

        // 2. Iterate and cache
        for (auto& kv : itemsTable) {
            std::string key = kv.first.as<std::string>();
            sol::table data = kv.second;
            LoadSingleItemFromLua(key, data);
            count++;
        }
        std::cout << "Loaded " << count << " items." << std::endl;
    }
    catch (const sol::error& e) {
        std::cerr << "Lua Error in items_master: " << e.what() << std::endl;
    }
}

void ItemFactory::LoadSingleItemFromLua(const std::string& key, sol::table& t) {
    ItemTemplate tpl;

    // Basic Fields
    tpl.name = t.get_or<std::string>("name", "Unknown Item");
    tpl.description = t.get_or<std::string>("desc", "...");
    tpl.symbol = t.get_or<std::string>("symbol", "("); // Default item symbol
    tpl.color = t.get_or<std::string>("color", "&w");
    tpl.weight = t.get_or("weight", 1);
    tpl.value = t.get_or("value", 0);
    tpl.itemType = t.get_or<std::string>("type", "misc");

    // Extract Type-Specific Data into JSON 'extra'
    if (tpl.itemType == "weapon") {
        sol::table dmg = t["damage"];
        tpl.extra["min_dmg"] = dmg.get_or("min", 1);
        tpl.extra["max_dmg"] = dmg.get_or("max", 1);
        tpl.extra["dmg_type"] = t.get_or<std::string>("dmg_type", "blunt");
    }
    else if (tpl.itemType == "armour") {
        tpl.extra["ac"] = t.get_or("ac", 0);
        tpl.extra["slot"] = t.get_or<std::string>("slot", "torso");
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

    // 1. Apply Overrides to Basic Data
    std::string name = overrides.value("name", tpl.name);
    std::string desc = overrides.value("desc", tpl.description);

    // 2. Add Basic Components
    ctx.registry->AddComponent<NameComponent>(id, { name });
    ctx.registry->AddComponent<DescriptionComponent>(id, { desc });
    ctx.registry->AddComponent<WeightComponent>(id, { tpl.weight });
    ctx.registry->AddComponent<ValueComponent>(id, { tpl.value });
    ctx.registry->AddComponent<VisualComponent>(id, { tpl.symbol, tpl.color });
    ctx.registry->AddComponent<ItemComponent>(id, { tpl.itemType });

    if (roomID == -1) {
        ctx.registry->AddComponent<PositionComponent>(id, { x,y,roomID });
    }

    // 3. Add Type Specifics
    AttachTypeComponents(id, tpl, overrides);

    return id;
}

void ItemFactory::AttachTypeComponents(int id, const ItemTemplate& tpl, const json& overrides) {
    if (tpl.itemType == "weapon") {
        WeaponComponent wc;
        // Check overrides, fallback to template JSON
        wc.minDamage = overrides.value("min_dmg", tpl.extra.value("min_dmg", 1));
        wc.maxDamage = overrides.value("max_dmg", tpl.extra.value("max_dmg", 2));
        wc.damageType = overrides.value("dmg_type", tpl.extra.value("dmg_type", "blunt"));
        ctx.registry->AddComponent<WeaponComponent>(id, wc);
    }
    else if (tpl.itemType == "armour") {
        ArmourComponent ac;
        ac.defense = overrides.value("ac", tpl.extra.value("ac", 0));
        std::string slot = overrides.value("slot", tpl.extra.value("slot", "torso"));
        EquipmentSlot eqSlot = TextHelperFunctions::StringToSlot(slot);
        ac.slot = eqSlot;
        ctx.registry->AddComponent<ArmourComponent>(id, ac);
    }
}