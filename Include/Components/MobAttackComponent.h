#pragma once
#include <sol/sol.hpp>
#include <string>
#include <vector>

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
