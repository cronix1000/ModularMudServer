#pragma once
#include "GameContext.h"
#include <map>
#include <iostream>
#include <nlohmann/json.hpp>
#include "Registry.h"
#include "Component.h"

using json = nlohmann::json;

struct ItemTemplate {
    std::string id;
    std::string name;
    std::string description;
    std::string symbol;
    std::string color;
    std::string itemType;
    int weight;
    int value;
    bool equippable;
    std::string primarySkill;
    std::vector<std::string> extraSkills;
    int minDamage;
    int maxDamage;
    std::string damageType;
    int defense;
    std::string slot;
    std::string script;
    json extra;
};

class ItemFactory {
public:
    GameContext& ctx;
    std::map<std::string, ItemTemplate> itemTemplates;

    ItemFactory(GameContext& g) : ctx(g) {}

    void LoadItemTemplatesFromJSON(const std::string& path = "items.json");
    int CreateItem(std::string templateID, json overrides = json::object(), int x = -1, int y = -1, int roomID = -1);

private:
    void LoadSingleItemFromJSON(const std::string& key, const json& data);
    void AttachTypeComponents(int entityID, const ItemTemplate& tpl, const json& overrides);
};