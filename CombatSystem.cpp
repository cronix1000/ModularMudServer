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

    // Send combat messages
    auto* sourceClient = ctx.registry->GetComponent<ClientComponent>(sourceID);
    if (sourceClient && sourceClient->client) {
        auto* targetName = ctx.registry->GetComponent<NameComponent>(targetID);
        std::string targetNameStr = targetName ? targetName->displayName : "target";
        sourceClient->client->QueueMessage("You attack " + targetNameStr + " for " + 
                                          std::to_string(finalDamage) + " " + damageType + " damage!");
    }

    auto* targetClient = ctx.registry->GetComponent<ClientComponent>(targetID);
    if (targetClient && targetClient->client) {
        auto* sourceName = ctx.registry->GetComponent<NameComponent>(sourceID);
        std::string sourceNameStr = sourceName ? sourceName->displayName : "someone";
        targetClient->client->QueueMessage(sourceNameStr + " attacks you for " + 
                                         std::to_string(finalDamage) + " " + damageType + " damage!");
        
        if (targetStats->Health <= 0) {
            targetClient->client->QueueMessage("You have been defeated!");
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

    // Send heal messages
    auto* sourceClient = ctx.registry->GetComponent<ClientComponent>(sourceID);
    if (sourceClient && sourceClient->client && sourceID != targetID) {
        auto* targetName = ctx.registry->GetComponent<NameComponent>(targetID);
        std::string targetNameStr = targetName ? targetName->displayName : "target";
        sourceClient->client->QueueMessage("You heal " + targetNameStr + " for " + 
                                          std::to_string(actualHeal) + " health!");
    }

    auto* targetClient = ctx.registry->GetComponent<ClientComponent>(targetID);
    if (targetClient && targetClient->client) {
        if (sourceID == targetID) {
            targetClient->client->QueueMessage("You heal yourself for " + 
                                             std::to_string(actualHeal) + " health!");
        } else {
            auto* sourceName = ctx.registry->GetComponent<NameComponent>(sourceID);
            std::string sourceNameStr = sourceName ? sourceName->displayName
                : "someone";
            targetClient->client->QueueMessage(sourceNameStr + " heals you for " + 
                                             std::to_string(actualHeal) + " health!");
        }
    }
}

void CombatSystem::ProcessBuff(int sourceID, int targetID, const std::string& buffType, float magnitude)
{
    // For now, just send a message - buffs/debuffs would need a separate component system
    auto* sourceClient = ctx.registry->GetComponent<ClientComponent>(sourceID);
    if (sourceClient && sourceClient->client) {
        sourceClient->client->QueueMessage("You cast " + buffType + " on your target!");
    }

    auto* targetClient = ctx.registry->GetComponent<ClientComponent>(targetID);
    if (targetClient && targetClient->client) {
        targetClient->client->QueueMessage("You are affected by " + buffType + "!");
    }
    
    // TODO: Implement actual buff/debuff system with temporary stat modifiers
}
