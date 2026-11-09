#pragma once
#include <string>
struct ScheduledEventComponent {
    float timeLeft;

    //EventType type;         // Enum: CombatAttack, CraftFinish, Teleport
    std::string actionID;   // "heavy_slam" or "potion_brew"
    int targetID;           // The victim or the crafting table
};