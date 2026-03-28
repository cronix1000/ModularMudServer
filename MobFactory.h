#pragma once
#include "GameContext.h"
#include <map>
#include <nlohmann/json.hpp>
#include "Registry.h"
#include "MobAttackComponent.h"

using json = nlohmann::json;

struct MobTemplate {
    std::string id;
    std::string name;
    std::string description;
    std::string symbol;
    std::string color;
    int hp;
    int level;
    int str, dex, intel;
    std::string aiType;
    std::string lootTable;
    int attackDamage;
    float attackSpeed;
    float criticalChance;
    float criticalMultiplier;
    std::vector<MobAttackPattern> attackPatterns;
    std::string script;
    json extra;
};

class MobFactory {
public:
    GameContext& ctx;
    std::map<std::string, MobTemplate> mobTemplates;

    MobFactory(GameContext& g) : ctx(g) {}

    void LoadMobTemplatesFromJSON(const std::string& path = "mobs.json");
    int CreateMob(std::string templateID, json overrides = json::object(), int x = 0, int y = 0, int roomID = -1);

private:
    void LoadSingleMobFromJSON(const std::string& key, const json& data);
};