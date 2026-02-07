#pragma once
#include <vector>
#include <string>

enum class MutationType { Internal, External, Replacement, Additive };

struct BodyPart {
    std::string name;          // e.g., "Draconic Third Eye"
    MutationType type;         // Does it replace a part or add a new one?
    int energyCost = 0;        // Does it drain Mana/Stamina to maintain?

    // Links to the Modifier System we discussed
    std::vector<StatModifier> statBonuses;
};