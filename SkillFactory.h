#pragma once
#include "GameContext.h"
#include "Registry.h"
#include "ResourceCostComponent.h"
#include "CoolDownDefinitionComponent.h"
#include "NameComponent.h"
#include "SkillDefintionComponent.h"
#include <iostream>
#include <map>
#include <string>
#include <nlohmann/json.hpp>
#include "ScriptManager.h"

using json = nlohmann::json;

struct SkillCategory {
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::string> stats;
    float synergyBonus;
};

struct SkillTemplate {
    std::string id;
    std::string category;
    std::string name;
    std::string description;
    std::string type;
    std::string activation;
    std::string command;
    float cooldown;
    float windup;
    int staminaCost;
    int manaCost;
    int healthCost;
    std::string targeting;
    int range;
    std::string script;
};

class SkillFactory {
public:
    GameContext& ctx;
    std::map<std::string, int> skillLookup;
    std::map<std::string, SkillCategory> categories;

    SkillFactory(GameContext& g) : ctx(g) {}

    void LoadSkillsFromJSON(const std::string& path = "skills.json");

    int GetSkillID(const std::string& key) {
        if (skillLookup.find(key) != skillLookup.end()) {
            return skillLookup[key];
        }
        return -1;
    }

private:
    void LoadCategoriesFromJSON(const json& data);
    void LoadSingleSkillFromJSON(const std::string& key, const json& skillData);
    void CreateSkillEntity(const SkillTemplate& tpl);
};