#pragma once
#include <string>

struct WeaponComponent {
    std::string templateID;

    int minDamage = 0;
    int maxDamage = 0;
    std::string damageType = "blunt"; // poke, slash, chop

    // Scaling factors (0.0 to 1.0+)
    float strScaling = 0.0f;
    float dexScaling = 0.0f;
    float intScaling = 0.0f;

    std::string alignmentType = "none"; // chaos, fae
    float alignmentScaling = 0.0f;

    int maxRange = 1;

    std::string defaultSkillTemplate;
};