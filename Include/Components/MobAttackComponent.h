#pragma once
#include <vector>
#include <string>
#include <sol/sol.hpp>

struct MobAttackPattern {
    std::string verb;
    float multiplier;
    std::string damageType;
};

struct MobAttackComponent {
    int attackDamage = 5;
    float attackSpeed = 2.0f;
    float criticalChance = 0.05f;
    float criticalMultiplier = 1.5f;
    std::vector<MobAttackPattern> attackPatterns;
    
    // Lua hook (optional)
    sol::function luaAttackFunc;
    bool hasLuaHook = false;
};