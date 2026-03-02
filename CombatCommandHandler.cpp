#include "CombatCommandHandler.h"
#include "CommandRegistry.h"
#include "ClientConnection.h"
#include "GameContext.h"
#include "Registry.h"
#include "NameComponent.h"
#include "PositionComponent.h"
#include "AttackIntentComponent.h"
#include "SkillHolderComponent.h"
#include "SkillIntentComponent.h"
#include "StatComponent.h"

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

CommandResult CombatCommandHandler::HandleAttack(ClientConnection* client,
												  const std::vector<std::string>& params,
												  GameContext& ctx) {
	if (params.empty()) {
		return CommandResult::Failure("Attack who?");
	}

	EntityID playerID = client->playerEntityID;
	
	// Build target name from all parameters (multi-word targets)
	std::string targetName = BuildTargetName(params);

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

	// Find Target
	EntityID targetID = FindTarget(ctx, playerID, targetName);

	if (targetID != -1) {
		ctx.registry->AddComponent<SkillIntentComponent>(playerID, { skillID, targetID });
		return CommandResult::Success();
	}
	else {
		return CommandResult::Failure("You don't see any '" + targetName + "' here.");
	}
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
	
	std::string targetName = BuildTargetName(targetParams);

	// Lookup Skill
	int skillID = skillHolder->Lookup(skillName);
	if (skillID == -1) {
		return CommandResult::Failure("You don't know a skill named '" + skillName + "'.");
	}

	// Lookup Target
	EntityID targetID = -1;
	if (targetName.empty() || targetName == "self") {
		targetID = playerID;
	}
	else {
		targetID = FindTarget(ctx, playerID, targetName);
	}

	if (targetID != -1) {
		ctx.registry->AddComponent<SkillIntentComponent>(playerID, { skillID, targetID });
		return CommandResult::Success();
	}
	else {
		return CommandResult::Failure("You don't see '" + targetName + "' here.");
	}
}

CommandResult CombatCommandHandler::HandleKill(ClientConnection* client,
											 const std::vector<std::string>& params,
											 GameContext& ctx) {
	// Kill is just an alias for attack
	return HandleAttack(client, params, ctx);
}
