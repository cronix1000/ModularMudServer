#include "BehaviorSystem.h"
#include "EventBus.h"
#include "BehaviourComponent.h"
#include "MobComponent.h"
#include "AttackIntentComponent.h"
#include "GameContext.h"
#include "Registry.h"
#include "EventBus.h"

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
        if (!ctx.registry->GetComponent<AttackIntentComponent>(data.victimID)) {
            ctx.registry->AddComponent<AttackIntentComponent>(data.victimID, { data.attackerID });
        }
    }

    printf("%d, %d" , data.attackerID, data.victimID);
}