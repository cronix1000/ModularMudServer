#pragma once
#include "GameContext.h"
#include <map>
#include <nlohmann/json.hpp>
#include "Registry.h"
#include <sol/sol.hpp>

using json = nlohmann::json;

// Include necessary component headers

struct MobTemplate {
    std::string name;
    std::string description;
    std::string symbol;
    std::string color;
    int hp;
    int level;

    // Flattened Stats
    int str, dex, intel;

    // Logic
    std::string aiType;
    std::string lootTable;

    json extra; // For special flags, custom scripts
};

class MobFactory {
public:
    GameContext& ctx;
    std::map<std::string, MobTemplate> mobTemplates;

    MobFactory(GameContext& g) : ctx(g) {}

    void LoadMobTemplatesFromLua();
    int CreateMob(std::string templateID, json overrides = json::object(), int x = 0, int y = 0, int roomID = -1);

private:
    void LoadSingleMobFromLua(const std::string& key, sol::table& data);
};