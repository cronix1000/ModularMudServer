#pragma once
#include <cmath>
#include <map>
#include <string>

struct SkillProgress {
    int level = 1;
    int xp = 0;
    int uses = 0;
};

inline int XPForNextLevel(int level) {
    return static_cast<int>(100.0 * std::pow(static_cast<double>(level), 1.5));
}

// The "Spellbook" attached to the Player Entity
struct SkillHolderComponent {
    // The Input Map: "User Text" -> "Actual Skill Entity"
    // This allows multiple aliases for the same skill!
    std::map<std::string, int> skillAliases;

    std::map<int, double> cooldowns;

    // The Progress Map: "Skill Entity" -> XP/Level/Uses
    std::map<int, SkillProgress> skills;

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

    SkillProgress& GetOrCreate(int skillID) {
        return skills[skillID];
    }

    int GetMastery(int skillId) {
        auto it = skills.find(skillId);
        if (it != skills.end()) {
            return it->second.level;
        }
        return 1;
    }

    bool AddXP(int skillID, int amount) {
        SkillProgress& progress = skills[skillID];
        progress.uses++;
        progress.xp += amount;
        bool leveledUp = false;
        while (progress.xp >= XPForNextLevel(progress.level)) {
            progress.xp -= XPForNextLevel(progress.level);
            progress.level++;
            leveledUp = true;
        }
        return leveledUp;
    }
};
