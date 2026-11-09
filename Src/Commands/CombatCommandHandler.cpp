#include "CombatCommandHandler.h"
#include "CommandRegistry.h"
#include "ClientConnection.h"
#include "GameContext.h"
#include "Registry.h"
#include "CombatComponents.h"
#include "MovementComponents.h"
#include "PresentationComponents.h"
#include "StatsComponents.h"
#include <cctype>
#include <algorithm>
#include "TargetingUtils.h"
void CombatCommandHandler::RegisterAll(CommandRegistry& registry) {
    registry.RegisterWithAliases("attack", HandleAttack, {"kill", "a"}, PermissionLevel::Player);
}

CommandResult CombatCommandHandler::HandleAttack(ClientConnection* client,
                                                 const std::vector<std::string>& params,
                                                 GameContext& ctx) {
    if (params.empty()) {
        return CommandResult::Failure("Attack who?");
    }

    EntityID playerID = client->playerEntityID;
    auto [targetTokens, targetIndex] = TargetingUtils::ParseTarget(params);
    if (targetTokens.empty()) {
        return CommandResult::Failure("Attack who?");
    }

    auto* skillHolder = ctx.registry->GetComponent<SkillHolderComponent>(playerID);
    if (!skillHolder) {
        return CommandResult::Failure("You don't know how to fight!");
    }

    int skillID = skillHolder->Lookup("attack");
    if (skillID == -1) {
        return CommandResult::Failure("You have no attack skill ready.");
    }

    TargetingIntentComponent targetingIntent;
    targetingIntent.sourceID = playerID;
    targetingIntent.targetName = TargetingUtils::TokensToString(targetTokens);
    targetingIntent.targetIndex = targetIndex;
    targetingIntent.queryTokens = targetTokens;
    targetingIntent.skillID = skillID;
    targetingIntent.promptExpireTime = 30.0f;
    targetingIntent.elapsedTime = 0.0f;

    ctx.registry->AddComponent<TargetingIntentComponent>(playerID, targetingIntent);
    return CommandResult::Success();
}

CommandResult CombatCommandHandler::HandleKill(ClientConnection* client,
                                               const std::vector<std::string>& params,
                                               GameContext& ctx) {
    return HandleAttack(client, params, ctx);
}