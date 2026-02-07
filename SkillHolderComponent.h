#pragma once
#include <map>
#include <string>
// The "Spellbook" attached to the Player Entity
struct SkillHolderComponent {
    // The Input Map: "User Text" -> "Actual Skill Entity"
    // This allows multiple aliases for the same skill!
    std::map<std::string, int> skillAliases;

    std::map<int, double> cooldowns;

    // The Progress Map: "Skill Entity" -> "XP/Level"
    std::map<int, int> mastery;

    // Maps "Skill Entity" -> number of times uses 1000 sword swings 
    std::map<int, int> skillUses;

    // Helper to find what the user meant
    int Lookup(const std::string& input) {
        if (skillAliases.count(input)) return skillAliases[input];
        return -1; // "You don't know a skill by that name."
    }

    bool IsReady(int skillID, double currentGlobalTime) {
        if (cooldowns.find(skillID) == cooldowns.end()) return true;
        return currentGlobalTime >= cooldowns[skillID];
    }

    void SetCooldown(int skillID, double currentGlobalTime, float duration) {
        cooldowns[skillID] = currentGlobalTime + duration;
    }

    int GetMastery(int skillId) {
        if(mastery.find(skillId) != mastery.end()) {
            return mastery[skillId];
        }
        return 0; // Default mastery level
    }

};