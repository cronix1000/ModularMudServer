#pragma once
#include <string>
#include <vector>

struct CombatIntentComponent {
    int targetID;               // Who we're attacking/affecting
    int skillID;                // Which skill was used
    std::string actionType;     // "attack", "heal", "buff", "debuff", etc.
    float magnitude;            // Damage amount, heal amount, etc.
    std::string damageType;     // "physical", "fire", "ice", "heal", etc.
    std::string dataString;     // Extra data like "critical_hit", "poison", etc.
    std::vector<std::string> addedTags; // Additional effect tags
    int sourceID;              // Who initiated this combat action
};