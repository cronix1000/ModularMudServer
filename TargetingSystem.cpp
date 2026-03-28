#include "TargetingSystem.h"
#include "Registry.h"
#include "GameContext.h"
#include "TargetingIntentComponent.h"
#include "SkillIntentComponent.h"
#include "CombatIntentComponent.h"
#include "NameComponent.h"
#include "PositionComponent.h"
#include "StatComponent.h"
#include "ClientComponent.h"

#include <iostream>
#include <vector>
#include <cmath>

void TargetingSystem::Run(float deltaTime) {
    // Process all targeting intents
    for (EntityID sourceID : ctx.registry->view<TargetingIntentComponent>()) {
        auto* targeting = ctx.registry->GetComponent<TargetingIntentComponent>(sourceID);
        if (!targeting) continue;

        // Update elapsed time for prompt expiration
        targeting->elapsedTime += deltaTime;
        if (targeting->elapsedTime > targeting->promptExpireTime) {
            // Prompt expired
            auto* client = ctx.registry->GetComponent<ClientComponent>(sourceID);
            if (client) {
                GameMessage msg;
                msg.type = "targeting_expired";
                msg.consoleText = "Targeting prompt expired.";
                client->QueueGameMessage(msg);
            }
            ctx.registry->RemoveComponent<TargetingIntentComponent>(sourceID);
            continue;
        }

        // Handle position-based targeting (AoE)
        if (targeting->isPositionTarget) {
            // For position targeting, we don't need to resolve a specific entity
            // Create a combat intent for the position
            CombatIntentComponent combatIntent;
            combatIntent.sourceID = sourceID;
            combatIntent.targetID = -1; // Position target
            combatIntent.actionType = "attack";
            combatIntent.magnitude = 1.0f;
            combatIntent.damageType = "physical";
            combatIntent.attackOnce = true;
            
            ctx.registry->AddComponent<CombatIntentComponent>(sourceID, combatIntent);
            ctx.registry->RemoveComponent<TargetingIntentComponent>(sourceID);
            continue;
        }

        // Get source position
        auto* sourcePos = ctx.registry->GetComponent<PositionComponent>(sourceID);
        if (!sourcePos) {
            ctx.registry->RemoveComponent<TargetingIntentComponent>(sourceID);
            continue;
        }

        // Find all matching targets in range
        std::vector<EntityID> matchingTargets;
        for (EntityID targetID : ctx.registry->view<NameComponent>()) {
            if (targetID == sourceID) continue;

            auto* name = ctx.registry->GetComponent<NameComponent>(targetID);
            auto* targetPos = ctx.registry->GetComponent<PositionComponent>(targetID);
            auto* targetStats = ctx.registry->GetComponent<StatComponent>(targetID);

            if (!name || !targetPos || !targetStats) continue;
            if (targetStats->Health <= 0) continue;

            // Check name match (case insensitive)
            if (!name->Matches(targeting->targetName)) continue;

            // Check range (simplified - same room for melee, distance check for ranged)
            if (targeting->maxRange <= 0.0f) {
                // Melee range - must be same room
                if (targetPos->roomId != sourcePos->roomId) continue;
            } else {
                // Ranged - check distance
                float dx = targetPos->x - sourcePos->x;
                float dy = targetPos->y - sourcePos->y;
                float distance = std::sqrt(dx * dx + dy * dy);
                if (distance > targeting->maxRange) continue;
            }

            // Check line of sight if required
            if (targeting->requireLineOfSight) {
                // TODO: Implement raycast for LoS
                // For now, assume LoS is clear if in range
            }

            matchingTargets.push_back(targetID);
        }

        // Handle target selection
        if (matchingTargets.empty()) {
            // No targets found
            auto* client = ctx.registry->GetComponent<ClientComponent>(sourceID);
            if (client) {
                GameMessage msg;
                msg.type = "targeting_failed";
                msg.consoleText = "You don't see any '" + targeting->targetName + "' here.";
                client->QueueGameMessage(msg);
            }
            ctx.registry->RemoveComponent<TargetingIntentComponent>(sourceID);
        } else if (matchingTargets.size() == 1 || targeting->targetIndex == 1) {
            // Single target or specifically selected first
            EntityID targetID = matchingTargets[0];
            if (targeting->targetIndex > 1 && targeting->targetIndex <= matchingTargets.size()) {
                targetID = matchingTargets[targeting->targetIndex - 1];
            }

            // Resolve target - create skill intent for combat
            SkillIntentComponent skillIntent;
            skillIntent.skillId = -1; // Basic attack
            skillIntent.targetId = targetID;
            
            ctx.registry->AddComponent<SkillIntentComponent>(sourceID, skillIntent);
            ctx.registry->RemoveComponent<TargetingIntentComponent>(sourceID);
        } else {
            // Multiple targets - send prompt if not already sent
            if (targeting->elapsedTime <= deltaTime) { // First frame
                auto* client = ctx.registry->GetComponent<ClientComponent>(sourceID);
                if (client) {
                    std::string prompt = "Multiple " + targeting->targetName + " found:\n";
                    for (size_t i = 0; i < matchingTargets.size() && i < 5; i++) {
                        auto* name = ctx.registry->GetComponent<NameComponent>(matchingTargets[i]);
                        if (name) {
                            prompt += std::to_string(i + 1) + ". " + name->displayName + "\n";
                        }
                    }
                    prompt += "Type 'attack " + targeting->targetName + " <number>' to select.";
                    
                    GameMessage msg;
                    msg.type = "targeting_prompt";
                    msg.consoleText = prompt;
                    client->QueueGameMessage(msg);
                }
            }
        }
    }
}