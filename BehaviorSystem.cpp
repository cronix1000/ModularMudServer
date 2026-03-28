#include "BehaviorSystem.h"
#include "EventBus.h"
#include "BehaviourComponent.h"
#include "MobComponent.h"
#include "CombatStateComponent.h"
#include "StatComponent.h"
#include "PositionComponent.h"
#include "GameContext.h"
#include "Registry.h"
#include "EventBus.h"
#include <iostream>

void BehaviorSystem::SetupListeners()
{
    ctx.eventBus->Subscribe(EventType::CombatHit, [this](const EventContext& ectx) {
        if (!std::holds_alternative<CombatEventData>(ectx.data)) return;
        const auto& data = std::get<CombatEventData>(ectx.data);

        OnEntityDamaged(ectx);
    });
}

void BehaviorSystem::OnEntityDamaged(const EventContext& ectx) {
    auto data = std::get<CombatEventData>(ectx.data);

    // 1. Is the victim a mob? If not, it doesn't have AI behavior.
    if (!ctx.registry->GetComponent<MobComponent>(data.victimID)) {
        return;
    }

    // 2. Get the behavior
    auto* behavior = ctx.registry->GetComponent<BehaviourComponent>(data.victimID);
    if (!behavior) return;

    // 3. React based on type
    if (behavior->behaviourType == BehaviourType::aggressive) {
        // Only fight back if we aren't already fighting someone
        if (!ctx.registry->GetComponent<CombatStateComponent>(data.victimID)) {
            CombatStateComponent combatState;
            combatState.targetID = data.attackerID;
            combatState.isPlayer = false;
            combatState.active = true;
            ctx.registry->AddComponent<CombatStateComponent>(data.victimID, combatState);
        }
    }
}

void BehaviorSystem::Run(float deltaTime) {
    // AI decision making for mobs with combat state
    // This runs before CombatStateSystem to allow AI to make decisions
    for (EntityID mobID : ctx.registry->view<CombatStateComponent>()) {
        auto* combatState = ctx.registry->GetComponent<CombatStateComponent>(mobID);
        if (!combatState || combatState->isPlayer) continue;

        // Basic AI: just validate target and continue attacking
        // More complex AI (fleeing, switching targets) can be added here
        
        // Check if target is valid
        if (combatState->targetID <= 0) {
            ctx.registry->RemoveComponent<CombatStateComponent>(mobID);
            continue;
        }

        auto* targetStats = ctx.registry->GetComponent<StatComponent>(combatState->targetID);
        if (!targetStats || targetStats->Health <= 0) {
            ctx.registry->RemoveComponent<CombatStateComponent>(mobID);
            continue;
        }

        // Check if in same room
        auto* mobPos = ctx.registry->GetComponent<PositionComponent>(mobID);
        auto* targetPos = ctx.registry->GetComponent<PositionComponent>(combatState->targetID);
        if (!mobPos || !targetPos || mobPos->roomId != targetPos->roomId) {
            ctx.registry->RemoveComponent<CombatStateComponent>(mobID);
            continue;
        }
        
        // AI will continue attacking via CombatStateSystem
    }
}
