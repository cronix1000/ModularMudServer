#include "CombatStateSystem.h"
#include "Registry.h"
#include "GameContext.h"
#include "CombatStateComponent.h"
#include "CombatIntentComponent.h"
#include "SkillIntentComponent.h"
#include "StatComponent.h"
#include "PositionComponent.h"
#include "EquipmentComponent.h"
#include "WeaponComponent.h"
#include "SkillHolderComponent.h"
#include "ItemComponent.h"
#include "ClientComponent.h"
#include "MobComponent.h"
#include "MobAttackComponent.h"
#include "EquipmentSlot.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>

float CalculateAttackInterval(float baseWindup, const std::string& scalingStat, int statValue) {
    // Logarithmic scaling: reduction = 1 - (1 / (1 + stat * 0.02))
    // At 50 stat: ~50% reduction, at 100 stat: ~67% reduction, caps around 80%
    float reduction = 1.0f - (1.0f / (1.0f + statValue * 0.02f));
    reduction = (std::min)(reduction, 0.8f); // Cap at 80% reduction
    return baseWindup * (1.0f - reduction);
}

void CombatStateSystem::Run(float deltaTime) {
    // Random number generator for crits and pattern selection
    static std::mt19937 rng(static_cast<unsigned>(time(nullptr)));
    
    for (EntityID entityID : ctx.registry->view<CombatStateComponent>()) {
        auto* combatState = ctx.registry->GetComponent<CombatStateComponent>(entityID);
        if (!combatState || !combatState->active) continue;

        // Decrement attack timer
        combatState->attackTimer -= deltaTime;

        if (combatState->attackTimer > 0.0f) {
            continue; // Still on cooldown
        }

        // Check if target is still valid
        if (combatState->targetID <= 0) {
            ctx.registry->RemoveComponent<CombatStateComponent>(entityID);
            continue;
        }

        auto* targetStats = ctx.registry->GetComponent<StatComponent>(combatState->targetID);
        if (!targetStats || targetStats->Health <= 0) {
            // Target died
            if (combatState->isPlayer) {
                auto* client = ctx.registry->GetComponent<ClientComponent>(entityID);
                if (client) {
                    GameMessage msg;
                    msg.type = "combat_end";
                    msg.consoleText = "Your target has been defeated.";
                    client->QueueGameMessage(msg);
                }
            }
            ctx.registry->RemoveComponent<CombatStateComponent>(entityID);
            continue;
        }

        // Check if still in range
        auto* entityPos = ctx.registry->GetComponent<PositionComponent>(entityID);
        auto* targetPos = ctx.registry->GetComponent<PositionComponent>(combatState->targetID);
        
        if (!entityPos || !targetPos || entityPos->roomId != targetPos->roomId) {
            // Target moved away
            if (combatState->isPlayer) {
                auto* client = ctx.registry->GetComponent<ClientComponent>(entityID);
                if (client) {
                    GameMessage msg;
                    msg.type = "combat_end";
                    msg.consoleText = "Your target is no longer in range.";
                    client->QueueGameMessage(msg);
                }
            }
            ctx.registry->RemoveComponent<CombatStateComponent>(entityID);
            continue;
        }

        // Check if this is a mob (uses different attack system)
        auto* mobAttack = ctx.registry->GetComponent<MobAttackComponent>(entityID);
        auto* mobComp = ctx.registry->GetComponent<MobComponent>(entityID);
        
        if (mobAttack && mobComp) {
            // MOB ATTACK SYSTEM
            float attackInterval = mobAttack->attackSpeed;
            
            // Check for Lua hook first (boss/special mobs)
            if (mobAttack->hasLuaHook && mobAttack->luaAttackFunc.valid()) {
                auto result = mobAttack->luaAttackFunc(entityID, combatState->targetID);
                if (result.valid()) {
                    sol::table attackData = result;
                    std::string verb = attackData.get_or<std::string>("verb", "attacks");
                    int damage = attackData.get_or("damage", mobAttack->attackDamage);
                    std::string damageType = attackData.get_or<std::string>("damage_type", "physical");
                    bool isCritical = attackData.get_or("is_critical", false);
                    
                    // Create combat intent from Lua result
                    CombatIntentComponent combatIntent;
                    combatIntent.sourceID = entityID;
                    combatIntent.targetID = combatState->targetID;
                    combatIntent.actionType = "attack";
                    combatIntent.magnitude = damage;
                    combatIntent.damageType = damageType;
                    combatIntent.dataString = verb;
                    if (isCritical) {
                        combatIntent.addedTags.push_back("critical");
                    }
                    
                    ctx.registry->AddComponent<CombatIntentComponent>(entityID, combatIntent);
                }
            } else {
                // STAT-BASED ATTACK SYSTEM
                auto* mobStats = ctx.registry->GetComponent<StatComponent>(entityID);
                if (mobStats) {
                    // Pick random attack pattern
                    std::uniform_int_distribution<int> patternDist(0, mobAttack->attackPatterns.size() - 1);
                    int patternIdx = patternDist(rng);
                    const auto& pattern = mobAttack->attackPatterns[patternIdx];
                    
                    // Calculate critical hit chance: base + (dexterity * 0.001)
                    float critChance = mobAttack->criticalChance + (mobStats->Dexterity * 0.001f);
                    critChance = (std::min)(critChance, 0.25f); // Cap at 25%
                    
                    std::uniform_real_distribution<float> critDist(0.0f, 1.0f);
                    bool isCritical = critDist(rng) < critChance;
                    
                    // Calculate damage
                    float damage = mobAttack->attackDamage * pattern.multiplier;
                    if (isCritical) {
                        damage *= mobAttack->criticalMultiplier;
                    }
                    
                    // Create combat intent
                    CombatIntentComponent combatIntent;
                    combatIntent.sourceID = entityID;
                    combatIntent.targetID = combatState->targetID;
                    combatIntent.actionType = "attack";
                    combatIntent.magnitude = static_cast<int>(damage);
                    combatIntent.damageType = pattern.damageType;
                    combatIntent.dataString = pattern.verb;
                    if (isCritical) {
                        combatIntent.addedTags.push_back("critical");
                    }
                    
                    ctx.registry->AddComponent<CombatIntentComponent>(entityID, combatIntent);
                }
            }
            
            // Reset attack timer
            combatState->attackTimer = attackInterval;
        } else {
            // PLAYER ATTACK SYSTEM (use weapon/skill system)
            float attackInterval = 1.0f; // Default
            auto* equipment = ctx.registry->GetComponent<EquipmentComponent>(entityID);
            auto* stats = ctx.registry->GetComponent<StatComponent>(entityID);
            
            if (equipment && stats) {
                int weaponID = equipment->slots[EquipmentSlot::mainArm];
                if (weaponID > 0) {
                    auto* weapon = ctx.registry->GetComponent<WeaponComponent>(weaponID);
                    if (weapon) {
                        int statValue = 0;
                        if (weapon->scalingStat == "strength") statValue = stats->Strength;
                        else if (weapon->scalingStat == "dexterity") statValue = stats->Dexterity;
                        else if (weapon->scalingStat == "intelligence") statValue = stats->Intelligence;
                        
                        attackInterval = CalculateAttackInterval(weapon->baseWindup, weapon->scalingStat, statValue);
                    }
                }
            }

            // Find primary skill from weapon
            int primarySkillID = -1;
            if (equipment) {
                int weaponID = equipment->slots[EquipmentSlot::mainArm];
                if (weaponID > 0) {
                    auto* itemComp = ctx.registry->GetComponent<ItemComponent>(weaponID);
                    if (itemComp) {
                        primarySkillID = itemComp->primarySkillId;
                    }
                }
            }

            // If no weapon equipped, check for unarmed skill
            if (primarySkillID == -1) {
                auto* skillHolder = ctx.registry->GetComponent<SkillHolderComponent>(entityID);
                if (skillHolder) {
                    primarySkillID = skillHolder->Lookup("attack");
                }
            }

            // Create skill intent for primary attack
            if (primarySkillID != -1) {
                SkillIntentComponent skillIntent;
                skillIntent.skillId = primarySkillID;
                skillIntent.targetId = combatState->targetID;
                
                ctx.registry->AddComponent<SkillIntentComponent>(entityID, skillIntent);
            }

            // Reset attack timer
            combatState->attackTimer = attackInterval;
        }
    }
}
