#pragma once
#include <string>
#include <vector>
struct SkillContext {
    int sourceID;       // The Player
    int targetID;       // The Victim / Anvil / Tree
    int skillID;        // The specific skill being used (allows JSON lookups)

    int masteryLevel;   // The player's skill level (1-100)
    int basePower;      // Context dependent: Weapon Dmg, Tool Tier, or Station Level
};


struct SkillResult {
    bool success;
    int skillID;            // Which skill was used (added for combat intent)
    std::string actionType; // "attack", "craft", "gather"
    float magnitude;        // Damage Mult / Craft Quality / Gather Amount
    std::string damageType; // "fire", "physical" (or product ID for crafting)
    std::string dataString; // Extra data (e.g. "critical_hit")
    std::vector<std::string> addedTags;
};