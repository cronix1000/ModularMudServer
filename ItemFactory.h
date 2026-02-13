#pragma once
#include "GameContext.h"
#include <map>
#include <iostream>
#include <nlohmann/json.hpp>
#include "Registry.h"
#include "Component.h" 
#include "sol/sol.hpp"


using json = nlohmann::json;

// The Blueprint
struct ItemTemplate {
    std::string name;
    std::string description;
    std::string symbol;
    std::string color;
    std::string itemType; // "weapon", "armour", "consumable", "misc"
    int weight;
    int value;

    json extra;
};

class ItemFactory {
public:
    GameContext& ctx;
    std::map<std::string, ItemTemplate> itemTemplates;

    ItemFactory(GameContext& g) : ctx(g) {}

    void LoadItemTemplatesFromLua();
    int CreateItem(std::string templateID, json overrides = json::object(), int x = -1, int y = -1, int roomID = -1);

private:
    void LoadSingleItemFromLua(const std::string& key, sol::table& data);
    void AttachTypeComponents(int entityID, const ItemTemplate& tpl, const json& overrides);
};