#include "BehaviorSystem.h"
#include "EventBus.h"
#include "ActorComponents.h"
#include "CombatComponents.h"
#include "MovementComponents.h"
#include "StatsComponents.h"
#include "GameContext.h"
#include "Registry.h"
#include "MobAttackBuilder.h"
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
        if (ctx.registry->GetComponent<CombatIntentComponent>(data.victimID)) {
            return;
        }

        CombatIntentComponent combatIntent;
        combatIntent.sourceID = data.victimID;
        combatIntent.targetID = data.attackerID;
        combatIntent.skillID = -1;
        combatIntent.actionType = "attack";
        combatIntent.magnitude = 1.0f;
        combatIntent.attackOnce = false;
        combatIntent.nextFireTime = 0.0;

        MobAttackRoll roll = MobAttackBuilder::BuildAttack(ctx.registry.get(), data.victimID, data.attackerID);
        if (roll.valid) {
            combatIntent.magnitude = roll.multiplier;
            combatIntent.damageType = roll.damageType;
            combatIntent.dataString = roll.verb;
        }

        ctx.registry->AddComponent<CombatIntentComponent>(data.victimID, combatIntent);
    }
}

void BehaviorSystem::Run(float deltaTime) {
    // Target validation and attack cadence are handled by CombatSystem's
    // renewing combat intents.
}
