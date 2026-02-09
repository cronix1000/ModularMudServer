#pragma once
#include "GameContext.h"
#include <fstream>
#include <iostream>  
#include <map>
#include <nlohmann/json.hpp>
#include "Component.h" // Assuming Health, Name, Position, etc. are here

using json = nlohmann::json;

struct MobTemplate {
    std::string name = "unknown";
    std::string symbol = "?";
    std::string color = "white";
    std::string description = "";
    StatComponent baseStats;
    std::string behave = "passive";
    nlohmann::json extra; // Stores any additional component data
};

class MobFactory {
    static BehaviourType StringToBehaviour(const std::string& str) {
        static const std::unordered_map<std::string, BehaviourType> stringToEnum = {
            {"passive",     BehaviourType::passive},
            {"aggressive",  BehaviourType::aggressive},
            {"neutral", BehaviourType::neutral}
        };

        auto it = stringToEnum.find(str);
        if (it != stringToEnum.end()) {
            return it->second;
        }

        return BehaviourType::neutral; // Default fallback
    }

public:
    GameContext& ctx;
    std::map<std::string, MobTemplate> mobTemplates;

    MobFactory(GameContext& g) : ctx(g) {}
    ~MobFactory() = default;

    void LoadMobTemplates(const std::string& jsonPath) {
        std::ifstream file(jsonPath);
        if (!file.is_open()) {
            std::cerr << "Failed to open mob file: " << jsonPath << std::endl;
            return;
        }

        json mobData;
        try {
            file >> mobData;
            if (mobData.is_object()) {
                for (auto& [key, val] : mobData.items()) {
                    if (key.empty()) continue;

                    MobTemplate mob;
                    mob.name = val.value("name", "unknown mob");
                    mob.symbol = val.value("char", "M");
                    mob.color = val.value("color", "white");
                    mob.description = val.value("description", "");
                    mob.behave= val.value("ai", "passive");

                    if (val.contains("stat")) {
                        auto& s = val["stat"];
                        // Only map what is in the JSON; others stay at StatComponent defaults
                        mob.baseStats.MaxHealth = s.value("health", 10);
                        mob.baseStats.Health = mob.baseStats.MaxHealth;
                        mob.baseStats.Armour = s.value("armour", 0);
                        mob.baseStats.AttackDamage = s.value("attack", 1);
                        mob.baseStats.DamageType = s.value("damage_type", "blunt");
                        mob.baseStats.DefenseType = s.value("defense_type", "soft");
                    }

                    // Store components block for flexible data loading
                    mob.extra = val.value("components", nlohmann::json::object());

                    mobTemplates.emplace(key, mob);
                }
            }
        }
        catch (const json::exception& e) {
            std::cerr << "JSON Error in " << jsonPath << ": " << e.what() << std::endl;
        }
    }

    EntityID Spawn(std::string templateID, int x, int y, int roomId, float respawnTimer = 0, bool persistant = true,json overrides = nullptr) {
        auto it = mobTemplates.find(templateID);
        if (it == mobTemplates.end()) return -1;

        EntityID id = ctx.registry->CreateEntity();
        ctx.registry->AddComponent<MobComponent>(id, MobComponent{ templateID,respawnTimer,persistant });
        AttachComponents(id, it->second, overrides, x, y, roomId);
        return id;
    }

private:
    void AttachComponents(EntityID id, const MobTemplate& t, json overrides, int x, int y, int roomId) {
        // 1. Core Mob Components
        ctx.registry->AddComponent<NameComponent>(id, NameComponent( t.name ));
        ctx.registry->AddComponent<VisualComponent>(id, VisualComponent{ t.symbol, t.color });
        ctx.registry->AddComponent<PositionComponent>(id, PositionComponent{ x, y, roomId });


        // 2. Extra Logic and Overrides
        json finalData = t.extra;
        if (overrides != nullptr) finalData.merge_patch(overrides);

        // Behaviour
        std::string behaviorStr = t.behave;
        BehaviourType type = StringToBehaviour(behaviorStr);
        ctx.registry->AddComponent<BehaviourComponent>(id, { type });


        // Start with the template's base stats
        StatComponent finalStats = t.baseStats;

        // Apply potential overrides from the specific Spawn call
        if (overrides != nullptr && overrides.contains("stat")) {
            auto& s = overrides["stat"];
            finalStats.MaxHealth = s.value("health", finalStats.MaxHealth);
            finalStats.Health = finalStats.MaxHealth;
            finalStats.AttackDamage = s.value("attack", finalStats.AttackDamage);
        }

        ctx.registry->AddComponent<StatComponent>(id, finalStats);

        if (finalData.contains("faction")) {
            // ctx.registry->AddComponent<FactionComponent>(id, { finalData["faction"] });
        }





    }
};

