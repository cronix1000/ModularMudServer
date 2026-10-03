#include "CombatCommandHandler.h"
#include "CommandRegistry.h"
#include "ClientConnection.h"
#include "GameContext.h"
#include "Registry.h"
#include "NameComponent.h"
#include "PositionComponent.h"
#include "TargetingIntentComponent.h"
#include "CombatStateComponent.h"
#include "SkillHolderComponent.h"
#include "SkillIntentComponent.h"
#include "StatComponent.h"
#include <cctype>
#include <algorithm>

namespace {
    std::pair<std::vector<std::string>, int> ParseTarget(const std::vector<std::string>& params) {
        if (params.empty()) return {{}, 0};
        const std::string& last = params.back();
        bool isNumber = !last.empty() && std::all_of(last.begin(), last.end(), ::isdigit);
        if (isNumber && params.size() > 1) {
            int idx = std::stoi(last);
            std::vector<std::string> tokens(params.begin(), params.end() - 1);
            return {tokens, idx};
        }
        return {params, 0};
    }

    std::string TokensToString(const std::vector<std::string>& tokens) {
        std::string out;
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (i) out += ' ';
            out += tokens[i];
        }
        return out;
    }
}

void CombatCommandHandler::RegisterAll(CommandRegistry& registry) {
    registry.RegisterWithAliases("attack", HandleAttack, {"kill", "a"}, PermissionLevel::Player);
    registry.RegisterWithAliases("cast", HandleCast, {"use"}, PermissionLevel::Player);
}

CommandResult CombatCommandHandler::HandleAttack(ClientConnection* client,
                                                 const std::vector<std::string>& params,
                                                 GameContext& ctx) {
    if (params.empty()) {
        return CommandResult::Failure("Attack who?");
    }

    EntityID playerID = client->playerEntityID;
    auto [targetTokens, targetIndex] = ParseTarget(params);
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
    targetingIntent.targetName = TokensToString(targetTokens);
    targetingIntent.targetIndex = targetIndex;
    targetingIntent.queryTokens = targetTokens;
    targetingIntent.skillID = skillID;
    targetingIntent.promptExpireTime = 30.0f;
    targetingIntent.elapsedTime = 0.0f;

    ctx.registry->AddComponent<TargetingIntentComponent>(playerID, targetingIntent);
    return CommandResult::Success();
}

CommandResult CombatCommandHandler::HandleCast(ClientConnection* client,
                                               const std::vector<std::string>& params,
                                               GameContext& ctx) {
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
    auto [targetTokens, targetIndex] = ParseTarget(targetParams);

    int skillID = skillHolder->Lookup(skillName);
    if (skillID == -1) {
        return CommandResult::Failure("You don't know a skill named '" + skillName + "'.");
    }

    if (targetTokens.empty() || (targetTokens.size() == 1 && targetTokens[0] == "self")) {
        ctx.registry->AddComponent<SkillIntentComponent>(playerID, { skillID, playerID });
        return CommandResult::Success();
    }

    TargetingIntentComponent targetingIntent;
    targetingIntent.sourceID = playerID;
    targetingIntent.targetName = TokensToString(targetTokens);
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