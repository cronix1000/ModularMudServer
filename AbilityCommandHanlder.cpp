#include "AbilityCommandHanlder.h"
#include "Registry.h"
#include "CommandRegistry.h"
#include "ClientConnection.h"
#include "CombatComponents.h"
#include "GameContext.h"
#include "CombatCommandHandler.h"
#include "TargetingUtils.h"
void AbilityCommandHandler::RegisterAll(CommandRegistry& registry) {
    registry.RegisterWithAliases("attack", HandleAbilityCall, { }, PermissionLevel::Player);
    registry.RegisterWithAliases("cast", HandleCastCall, { "" }, PermissionLevel::Player);
}

CommandResult AbilityCommandHandler::HandleAbilityCall(ClientConnection* client, const std::vector<std::string>& params, GameContext& ctx)
{
    if (params.empty()) {
        return CommandResult::Failure("Cast what?");
    }

    EntityID playerID = client->playerEntityID;
    auto* skillHolder = ctx.registry->GetComponent<SkillHolderComponent>(playerID);
    if (!skillHolder) {
        return CommandResult::Failure("You don't know any skills.");
    }

    std::string skillName = params[0];
    std::vector<std::string> targetParams(params.begin() + 1, params.end());
    auto [targetTokens, targetIndex] = TargetingUtils::ParseTarget(targetParams);


    int skillID = skillHolder->Lookup(skillName);
    if (skillID == -1) {
        return CommandResult::Failure("You don't know a skill named '" + skillName + "'.");
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

    // check alias list
    return CommandResult();
}

CommandResult AbilityCommandHandler::HandleCastCall(ClientConnection* client, const std::vector<std::string>& params, GameContext& ctx)
{
    // future for spells that are done with symbs ex. cast @@c those symbols create a spell

    return CommandResult();
}
