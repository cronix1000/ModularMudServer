#include "StopCommandHandler.h"
#include "CombatStateComponent.h"
#include "SkillWindupComponents.h"
#include "ClientComponent.h"
#include "Registry.h"

void StopCommandHandler::RegisterAll(CommandRegistry& registry) {
    registry.Register("stop", HandleStop, PermissionLevel::Player);
}

CommandResult StopCommandHandler::HandleStop(ClientConnection* client,
                                            const std::vector<std::string>& params,
                                            GameContext& ctx) {
    EntityID playerID = client->playerEntityID;
    
    // Check if player is in combat state
    auto* combatState = ctx.registry->GetComponent<CombatStateComponent>(playerID);
    if (combatState) {
        ctx.registry->RemoveComponent<CombatStateComponent>(playerID);
    }
    
    // Also cancel any skill windup
    if (ctx.registry->GetComponent<SkillWindupComponent>(playerID)) {
        ctx.registry->RemoveComponent<SkillWindupComponent>(playerID);
    }
    
    return CommandResult::Success("You stop fighting.");
}