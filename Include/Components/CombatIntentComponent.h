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
    bool attackOnce = false;
    double nextFireTime = 0.0; // Renewal timestamp for auto-attack intents; 0 = due now

    // One-shot skill results waiting to fire while a renewing auto-attack
    // intent occupies this component slot.
    std::vector<CombatIntentComponent> queuedOneShots;

    CombatIntentComponent() = default;
};