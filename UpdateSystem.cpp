#include "UpdateSystem.h"
#include "Registry.h"
#include "Component.h"
#include "GameContext.h"

#include <unordered_map>
#include <map>
using EntityID = int;

UpdateSystem::UpdateSystem(GameContext& gc) : ctx(gc)
{

}

UpdateSystem::~UpdateSystem()
{
}

void UpdateSystem::Run() {

}

void UpdateSystem::Update(float dt) {
    // Update all entities that are "busy".
    for (EntityID entity : ctx.registry->view<BusyComponent>()) {
        auto* busy = ctx.registry->GetComponent<BusyComponent>(entity);
        if (!busy) continue;

        busy->timeLeft -= dt;
        if (busy->timeLeft <= 0) {
            // Entity is no longer busy, remove the component.
            ctx.registry->RemoveComponent<BusyComponent>(entity);
        }
    }

    // Update all entities with a "pulse" (e.g., for health regeneration).
    for (EntityID entity : ctx.registry->view<PulseComponent>()) {
        auto* pulse = ctx.registry->GetComponent<PulseComponent>(entity);
        auto* stats = ctx.registry->GetComponent<StatComponent>(entity);
        if (!pulse || !stats) continue;

        pulse->timeSince += dt;
        if (pulse->timeSince >= 3.0f) {
            stats->Health = (std::min)(stats->MaxHealth, stats->Health + 5);
            pulse->timeSince = 0; // Reset the pulse timer.
        }
    }

    // Update all entities with a scheduled event.
    for (EntityID entity : ctx.registry->view<ScheduledEventComponent>()) {
        auto* evt = ctx.registry->GetComponent<ScheduledEventComponent>(entity);
        if (!evt) continue;

        evt->timeLeft -= dt;
        if (evt->timeLeft <= 0) {
            // The event's time is up, remove it.
            // Note: The original code did not trigger any event logic here.
            ctx.registry->RemoveComponent<ScheduledEventComponent>(entity);
        }
    }
}