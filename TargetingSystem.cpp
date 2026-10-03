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
#include "EntityResolver.h"

#include <iostream>
#include <vector>
#include <cmath>

namespace {
    std::vector<std::string> SplitBySpace(const std::string& s) {
        std::vector<std::string> out;
        std::string cur;
        for (char c : s) {
            if (c == ' ') {
                if (!cur.empty()) { out.push_back(cur); cur.clear(); }
            } else {
                cur.push_back(c);
            }
        }
        if (!cur.empty()) out.push_back(cur);
        return out;
    }
}

void TargetingSystem::Run(float deltaTime) {
    for (EntityID sourceID : ctx.registry->view<TargetingIntentComponent>()) {
        auto* targeting = ctx.registry->GetComponent<TargetingIntentComponent>(sourceID);
        if (!targeting) continue;

        targeting->elapsedTime += deltaTime;
        if (targeting->elapsedTime > targeting->promptExpireTime) {
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

        if (targeting->isPositionTarget) {
            CombatIntentComponent combatIntent;
            combatIntent.sourceID = sourceID;
            combatIntent.targetID = -1;
            combatIntent.actionType = "attack";
            combatIntent.magnitude = 1.0f;
            combatIntent.damageType = "physical";
            combatIntent.attackOnce = true;

            ctx.registry->AddComponent<CombatIntentComponent>(sourceID, combatIntent);
            ctx.registry->RemoveComponent<TargetingIntentComponent>(sourceID);
            continue;
        }

        auto* sourcePos = ctx.registry->GetComponent<PositionComponent>(sourceID);
        if (!sourcePos) {
            ctx.registry->RemoveComponent<TargetingIntentComponent>(sourceID);
            continue;
        }

        std::vector<EntityID> matchingTargets;

        // If we already have stored candidates (from a prior prompt), use them.
        if (!targeting->candidates.empty()) {
            matchingTargets = targeting->candidates;
        } else {
            // First pass: query the resolver.
            std::vector<std::string> query = targeting->queryTokens;
            if (query.empty() && !targeting->targetName.empty()) {
                query = SplitBySpace(targeting->targetName);
            }
            if (!query.empty() && ctx.entityFind) {
                if (targeting->maxRange > 0.0f) {
                    float r = targeting->maxRange;
                    matchingTargets = ctx.entityFind->FindByShape(
                        sourcePos->roomId, sourcePos->x, sourcePos->y,
                        query,
                        [r](int dx, int dy) {
                            float d = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                            return d >= 1.0f && d <= r;
                        });
                } else {
                    matchingTargets = ctx.entityFind->FindByShape(
                        sourcePos->roomId, sourcePos->x, sourcePos->y,
                        query, EntityResolver::ShapeAround());
                }
                // Exclude the source itself and dead mobs.
                std::vector<EntityID> filtered;
                filtered.reserve(matchingTargets.size());
                for (EntityID e : matchingTargets) {
                    if (e == sourceID) continue;
                    auto* stats = ctx.registry->GetComponent<StatComponent>(e);
                    if (stats && stats->Health <= 0) continue;
                    filtered.push_back(e);
                }
                matchingTargets = std::move(filtered);
            }
        }

        if (matchingTargets.empty()) {
            auto* client = ctx.registry->GetComponent<ClientComponent>(sourceID);
            if (client) {
                GameMessage msg;
                msg.type = "targeting_failed";
                msg.consoleText = "You don't see any '" + targeting->targetName + "' here.";
                client->QueueGameMessage(msg);
            }
            ctx.registry->RemoveComponent<TargetingIntentComponent>(sourceID);
        } else if (matchingTargets.size() == 1 || targeting->targetIndex >= 1) {
            EntityID targetID = matchingTargets[0];
            if (targeting->targetIndex >= 2 && targeting->targetIndex <= static_cast<int>(matchingTargets.size())) {
                targetID = matchingTargets[targeting->targetIndex - 1];
            }

            SkillIntentComponent skillIntent;
            skillIntent.skillId = targeting->skillID;
            skillIntent.targetId = targetID;

            ctx.registry->AddComponent<SkillIntentComponent>(sourceID, skillIntent);
            ctx.registry->RemoveComponent<TargetingIntentComponent>(sourceID);
        } else {
            // Multiple targets — prompt. On first frame, populate candidates so subsequent
            // numeric-reply commands can use them directly.
            targeting->candidates = matchingTargets;
            if (targeting->elapsedTime <= deltaTime) {
                auto* client = ctx.registry->GetComponent<ClientComponent>(sourceID);
                if (client) {
                    std::string prompt = "Multiple " + targeting->targetName + " found:\r\n";
                    for (size_t i = 0; i < matchingTargets.size() && i < 5; i++) {
                        auto* name = ctx.registry->GetComponent<NameComponent>(matchingTargets[i]);
                        if (name) {
                            prompt += std::to_string(i + 1) + ". " + name->displayName + "\r\n";
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