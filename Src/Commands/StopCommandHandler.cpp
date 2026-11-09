#include "StopCommandHandler.h"
#include "ClientConnection.h"
#include "CombatComponents.h"
#include "PlayerComponents.h"
#include "Registry.h"

void StopCommandHandler::RegisterAll(CommandRegistry& registry) {
    registry.Register("stop", HandleStop, PermissionLevel::Player);
}

CommandResult StopCommandHandler::HandleStop(ClientConnection* client,
                                            const std::vector<std::string>& params,
                                            GameContext& ctx) {
    EntityID playerID = client->playerEntityID;

    bool wasFighting = false;

    if (ctx.registry->GetComponent<CombatIntentComponent>(playerID)) {
        ctx.registry->RemoveComponent<CombatIntentComponent>(playerID);
        wasFighting = true;
    }

    if (ctx.registry->GetComponent<SkillWindupComponent>(playerID)) {
        ctx.registry->RemoveComponent<SkillWindupComponent>(playerID);
        wasFighting = true;
    }

    return CommandResult::Success(wasFighting ? "You stop fighting." : "You aren't fighting.");
}
