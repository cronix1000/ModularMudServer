#include "CombatSystem.h"
#include "GameContext.h"             
#include "Registry.h"                 
#include "CombatIntentComponent.h"
#include "StatComponent.h"
#include "ClientComponent.h"
#include "BusyComponent.h"
#include "NameComponent.h"
#include "EventBus.h"
#include <iostream>
#include <string>
#include <algorithm>
#include <random>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void CombatSystem::run()
{
    // Process all combat intents created by the skill system
    for (EntityID sourceID : ctx.registry->view<CombatIntentComponent>()) {
        auto* intent = ctx.registry->GetComponent<CombatIntentComponent>(sourceID);
        if (!intent) continue;

        // Check if source is busy
        auto* busy = ctx.registry->GetComponent<BusyComponent>(sourceID);
        if (busy && busy->timeLeft > 0) {
            continue; 
        }

        // Process the combat intent based on action type
        ProcessCombatIntent(sourceID, *intent);
        
        // Remove the intent after processing
        ctx.registry->RemoveComponent<CombatIntentComponent>(sourceID);
    }
}

void CombatSystem::ProcessCombatIntent(int sourceID, const CombatIntentComponent& intent)
{
    // Route to appropriate handler based on action type
    if (intent.actionType == "attack") {
        ProcessAttack(sourceID, intent.targetID, intent.magnitude, intent.damageType);
    }
    else if (intent.actionType == "heal") {
        ProcessHeal(sourceID, intent.targetID, intent.magnitude);
    }
    else if (intent.actionType == "buff" || intent.actionType == "debuff") {
        ProcessBuff(sourceID, intent.targetID, intent.actionType, intent.magnitude);
    }
    // Add more action types as needed
}

void CombatSystem::ProcessAttack(int sourceID, int targetID, float damage, const std::string& damageType)
{
    auto* targetStats = ctx.registry->GetComponent<StatComponent>(targetID);
    if (!targetStats || targetStats->Health <= 0) return;

    auto* sourceStats = ctx.registry->GetComponent<StatComponent>(sourceID);
    if (!sourceStats) return;

    // Calculate final damage with armor
    int finalDamage = (std::max)(1, (int)(damage * sourceStats->AttackDamage) - targetStats->Armour);
    
    // Apply damage
    targetStats->Health = (std::max)(0, targetStats->Health - finalDamage);

    // Send combat messages using new GameMessage pattern
    auto* sourceClient = ctx.registry->GetComponent<ClientComponent>(sourceID);
    if (sourceClient) {
        auto* targetName = ctx.registry->GetComponent<NameComponent>(targetID);
        std::string targetNameStr = targetName ? targetName->displayName : "target";
        
        json jsonData = {
            {"action", "attack"},
            {"damage", finalDamage},
            {"damage_type", damageType},
            {"target", targetNameStr},
            {"target_current_hp", targetStats->Health},
            {"target_max_hp", targetStats->MaxHealth}
        };
        
        GameMessage msg;
        msg.type = "combat_hit";
        msg.consoleText = "You attack " + targetNameStr + " for &R" + std::to_string(finalDamage) + "&X " + damageType + " damage!";
        msg.jsonData = jsonData.dump();
        sourceClient->QueueGameMessage(msg);
    }

    auto* targetClient = ctx.registry->GetComponent<ClientComponent>(targetID);
    if (targetClient) {
        auto* sourceName = ctx.registry->GetComponent<NameComponent>(sourceID);
        std::string sourceNameStr = sourceName ? sourceName->displayName : "someone";
        
        json jsonData = {
            {"action", "attacked"},
            {"damage", finalDamage},
            {"damage_type", damageType},
            {"source", sourceNameStr},
            {"current_hp", targetStats->Health},
            {"max_hp", targetStats->MaxHealth}
        };
        
        GameMessage msg;
        msg.type = "combat_hit";
        msg.consoleText = sourceNameStr + " attacks you for &R" + std::to_string(finalDamage) + "&X " + damageType + " damage!";
        msg.jsonData = jsonData.dump();
        targetClient->QueueGameMessage(msg);
        
        if (targetStats->Health <= 0) {
            json defeatData = {
                {"defeated_by", sourceNameStr},
                {"final_damage", finalDamage}
            };
            
            GameMessage defeatMsg;
            defeatMsg.type = "player_defeat";
            defeatMsg.consoleText = "&RYou have been defeated!&X";
            defeatMsg.jsonData = defeatData.dump();
            targetClient->QueueGameMessage(defeatMsg);
        }
    }

    // Apply recovery time to the attacker
    auto* sourceStatsForRecovery = ctx.registry->GetComponent<StatComponent>(sourceID);
    if (sourceStatsForRecovery) {
        float recovery = 20.0f / (float)sourceStatsForRecovery->attackSpeed;
        ctx.registry->AddComponent<BusyComponent>(sourceID, BusyComponent{ recovery });
    }

    // Fire combat event for other systems
    CombatEventData ectx = { sourceID, targetID };
    EventContext data;
    data.data = ectx;
    ctx.eventBus->Publish(EventType::CombatHit, data);
}

void CombatSystem::ProcessHeal(int sourceID, int targetID, float healAmount)
{
    auto* targetStats = ctx.registry->GetComponent<StatComponent>(targetID);
    if (!targetStats) return;

    int actualHeal = (std::min)((int)healAmount, targetStats->MaxHealth - targetStats->Health);
    targetStats->Health += actualHeal;

    // Send heal messages using new GameMessage pattern
    auto* sourceClient = ctx.registry->GetComponent<ClientComponent>(sourceID);
    if (sourceClient && sourceID != targetID) {
        auto* targetName = ctx.registry->GetComponent<NameComponent>(targetID);
        std::string targetNameStr = targetName ? targetName->displayName : "target";
        
        json jsonData = {
            {"action", "heal"},
            {"heal_amount", actualHeal},
            {"target", targetNameStr},
            {"target_current_hp", targetStats->Health},
            {"target_max_hp", targetStats->MaxHealth}
        };
        
        GameMessage msg;
        msg.type = "combat_heal";
        msg.consoleText = "You heal " + targetNameStr + " for &G" + std::to_string(actualHeal) + "&X health!";
        msg.jsonData = jsonData.dump();
        sourceClient->QueueGameMessage(msg);
    }

    auto* targetClient = ctx.registry->GetComponent<ClientComponent>(targetID);
    if (targetClient) {
        if (sourceID == targetID) {
            json jsonData = {
                {"action", "self_heal"},
                {"heal_amount", actualHeal},
                {"current_hp", targetStats->Health},
                {"max_hp", targetStats->MaxHealth}
            };
            
            GameMessage msg;
            msg.type = "combat_heal";
            msg.consoleText = "You heal yourself for &G" + std::to_string(actualHeal) + "&X health!";
            msg.jsonData = jsonData.dump();
            targetClient->QueueGameMessage(msg);
        } else {
            auto* sourceName = ctx.registry->GetComponent<NameComponent>(sourceID);
            std::string sourceNameStr = sourceName ? sourceName->displayName : "someone";
            
            json jsonData = {
                {"action", "healed"},
                {"heal_amount", actualHeal},
                {"source", sourceNameStr},
                {"current_hp", targetStats->Health},
                {"max_hp", targetStats->MaxHealth}
            };
            
            GameMessage msg;
            msg.type = "combat_heal";
            msg.consoleText = sourceNameStr + " heals you for &G" + std::to_string(actualHeal) + "&X health!";
            msg.jsonData = jsonData.dump();
            targetClient->QueueGameMessage(msg);
        }
    }
}

void CombatSystem::ProcessBuff(int sourceID, int targetID, const std::string& buffType, float magnitude)
{
    // For now, just send a message using new GameMessage pattern - buffs/debuffs would need a separate component system
    auto* sourceClient = ctx.registry->GetComponent<ClientComponent>(sourceID);
    if (sourceClient) {
        json jsonData = {
            {"action", "cast_buff"},
            {"buff_type", buffType},
            {"magnitude", magnitude}
        };
        
        GameMessage msg;
        msg.type = "combat_buff";
        msg.consoleText = "You cast &C" + buffType + "&X on your target!";
        msg.jsonData = jsonData.dump();
        sourceClient->QueueGameMessage(msg);
    }

    auto* targetClient = ctx.registry->GetComponent<ClientComponent>(targetID);
    if (targetClient) {
        json jsonData = {
            {"action", "buffed"},
            {"buff_type", buffType},
            {"magnitude", magnitude}
        };
        
        GameMessage msg;
        msg.type = "combat_buff";
        msg.consoleText = "You are affected by &C" + buffType + "&X!";
        msg.jsonData = jsonData.dump();
        targetClient->QueueGameMessage(msg);
    }
    
    // TODO: Implement actual buff/debuff system with temporary stat modifiers
}
