#pragma once
#include <string>
enum class StatType {
    Strength, Intelligence, Wisdom, Dexterity,
    MaxHealth, Armour, MagicDefense, AttackSpeed
};

struct StatModifier {
    std::string source;
    StatType target;   // Changed from string to Enum
    float value;
    bool isPercent;
};