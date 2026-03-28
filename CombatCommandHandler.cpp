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

EntityID CombatCommandHandler::FindTarget(GameContext& ctx, EntityID playerID, const std::string& targetName) {
	auto* playerPos = ctx.registry->GetComponent<PositionComponent>(playerID);
	if (!playerPos) return -1;

	auto& name_entities = ctx.registry->view<NameComponent>();
	auto& pos_entities = ctx.registry->view<PositionComponent>();

	// Iterate over the smaller of the two sets for efficiency
	if (name_entities.size() < pos_entities.size()) {
		for (EntityID id : name_entities) {
			if (id == playerID) continue;
			if (ctx.registry->HasComponent<PositionComponent>(id)) {
				auto* name = ctx.registry->GetComponent<NameComponent>(id);
				auto* pos = ctx.registry->GetComponent<PositionComponent>(id);
				if (pos->roomId == playerPos->roomId && name->Matches(targetName)) {
					return id;
				}
			}
		}
	}
	else {
		for (EntityID id : pos_entities) {
			if (id == playerID) continue;
			if (ctx.registry->HasComponent<NameComponent>(id)) {
				auto* name = ctx.registry->GetComponent<NameComponent>(id);
				auto* pos = ctx.registry->GetComponent<PositionComponent>(id);
				if (pos->roomId == playerPos->roomId && name->Matches(targetName)) {
					return id;
				}
			}
		}
	}

	return -1;
}

std::string CombatCommandHandler::BuildTargetName(const std::vector<std::string>& params) {
	if (params.empty()) return "";
	
	std::string targetName = "";
	for (size_t i = 0; i < params.size(); ++i) {
		targetName += params[i];
		if (i < params.size() - 1) targetName += " ";
	}
	return targetName;
}

void CombatCommandHandler::RegisterAll(CommandRegistry& registry) {
	// Basic attack command
	registry.RegisterWithAliases("attack", HandleAttack, {"kill", "a"}, PermissionLevel::Player);
	
	// Cast spell command
	registry.RegisterWithAliases("cast", HandleCast, {"use"}, PermissionLevel::Player);
}

// Parse target name and optional index from params
// e.g., ["goblin"] -> ("goblin", 0), ["goblin", "2"] -> ("goblin", 2)
std::pair<std::string, int> ParseTargetWithIndex(const std::vector<std::string>& params) {
	if (params.empty()) return {"", 0};
	
	// Check if last parameter is a number
	const std::string& lastParam = params.back();
	bool isNumber = !lastParam.empty() && std::all_of(lastParam.begin(), lastParam.end(), ::isdigit);
	
	if (isNumber && params.size() > 1) {
		// Has index: "goblin 2"
		int index = std::stoi(lastParam);
		std::string targetName = "";
		for (size_t i = 0; i < params.size() - 1; ++i) {
			targetName += params[i];
			if (i < params.size() - 2) targetName += " ";
		}
		return {targetName, index};
	} else {
		// No index: "goblin"
		std::string targetName = "";
		for (size_t i = 0; i < params.size(); ++i) {
			targetName += params[i];
			if (i < params.size() - 1) targetName += " ";
		}
		return {targetName, 0};
	}
}

CommandResult CombatCommandHandler::HandleAttack(ClientConnection* client,
											  const std::vector<std::string>& params,
											  GameContext& ctx) {
	if (params.empty()) {
		return CommandResult::Failure("Attack who?");
	}

	EntityID playerID = client->playerEntityID;
	
	// Parse target name and optional index
	auto [targetName, targetIndex] = ParseTargetWithIndex(params);
	
	if (targetName.empty()) {
		return CommandResult::Failure("Attack who?");
	}

	// Find Skill ID for "attack"
	auto* skillHolder = ctx.registry->GetComponent<SkillHolderComponent>(playerID);
	if (!skillHolder) {
		return CommandResult::Failure("You don't know how to fight!");
	}

	// "attack" is a reserved alias for the primary weapon skill
	int skillID = skillHolder->Lookup("attack");
	if (skillID == -1) {
		return CommandResult::Failure("You have no attack skill ready.");
	}

	// Create targeting intent - let TargetingSystem resolve multi-target ambiguity
	TargetingIntentComponent targetingIntent;
	targetingIntent.sourceID = playerID;
	targetingIntent.targetName = targetName;
	targetingIntent.targetIndex = targetIndex;
	
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

	// For now, assume format: cast <skill> <target>
	// TODO: Support multi-word skill names like "heavy slam"
	std::string skillName = params[0];
	
	std::vector<std::string> targetParams;
	for (size_t i = 1; i < params.size(); ++i) {
		targetParams.push_back(params[i]);
	}
	
	// Parse target with optional index
	auto [targetName, targetIndex] = ParseTargetWithIndex(targetParams);

	// Lookup Skill
	int skillID = skillHolder->Lookup(skillName);
	if (skillID == -1) {
		return CommandResult::Failure("You don't know a skill named '" + skillName + "'.");
	}

	if (targetName.empty() || targetName == "self") {
		// Self-targeted, no need for targeting system
		ctx.registry->AddComponent<SkillIntentComponent>(playerID, { skillID, playerID });
		return CommandResult::Success();
	}

	// Create targeting intent for target selection
	TargetingIntentComponent targetingIntent;
	targetingIntent.sourceID = playerID;
	targetingIntent.targetName = targetName;
	targetingIntent.targetIndex = targetIndex;
	// Store skill ID for later use after targeting resolves
	// This requires modifying TargetingIntentComponent to include skillID
	
	ctx.registry->AddComponent<TargetingIntentComponent>(playerID, targetingIntent);
	
	return CommandResult::Success();
}

CommandResult CombatCommandHandler::HandleKill(ClientConnection* client,
											 const std::vector<std::string>& params,
											 GameContext& ctx) {
	// Kill is just an alias for attack
	return HandleAttack(client, params, ctx);
}