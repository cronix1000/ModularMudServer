#pragma once
#include "StatModifier.h"

enum class MutationType {Replacement, Additive};
enum class BodyPartSlot {
    // --- Standard External Slots ---
    Head,           // e.g., Third Eye, Horns
    Eyes,           // e.g., Mystic Eyes, Dragon Sight
    Throat,         // e.g., Acid Gland, Dragon Cords
    Torso,          // e.g., Carapace, Extra Arms
    Back,           // e.g., Wings, Bone Spikes
    Arms,           // e.g., Scales, Claws
    Legs,           // e.g., Hooves, Digitigrade
    Tail,           // e.g., Scorpion Stinger

    // --- Internal / Metaphysical Slots ---
    Heart,          // e.g., Demon Heart, Mana Pump
    Lungs,          // e.g., Void Lungs (Breathe underwater/space)
    Blood,          // e.g., Vampire Bloodline, Ichor
    Bone,           // e.g., Adamantine Skeleton
    Brain,          // e.g., Neural Link, Parallel Processing
    Skin,           // e.g., Stone Skin, Adaptive Camo
};
struct BodyPart {
    std::string name;          // e.g., "Draconic Third Eye"
    MutationType type;         // Does it replace a part or add a new one?
    int energyCost = 0;        // Does it drain Mana/Stamina to maintain?

    // Links to the Modifier System we discussed
    std::vector<StatModifier> statBonuses;
};

struct BodyComponent
{
    std::map< BodyPartSlot, BodyPart> activeMutations;
};

