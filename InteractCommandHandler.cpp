#include "InteractCommandHandler.h"
#include "Registry.h"
#include "CommandRegistry.h"
#include "ClientConnection.h"
#include "GameContext.h"
#include "EntityResolver.h"
#include "PositionComponent.h"
#include "InteractableComponent.h"
#include "InteractableIntentComponent.h"


void InteractCommandHandler::RegisterAll(CommandRegistry& registry) {
	registry.RegisterWithAliases("interact", HandleInteract, {"use"}, PermissionLevel::Player);
	registry.RegisterWithAliases("pray", HandlePray, {}, PermissionLevel::Player);
	registry.RegisterWithAliases("open", HandleOpen, {}, PermissionLevel::Player);
	//registry.RegisterWithAliases("enter", HandleEnter, {}, PermissionLevel::Player); // for portals if I choose to make them not on touch
}

CommandResult InteractCommandHandler::HandleInteract(ClientConnection* client, const std::vector<std::string>& params, GameContext& ctx)
{
	if (params.empty()) {
		return CommandResult::Failure("Interact with what?");
	}

	EntityID playerID = client->playerEntityID;
	auto* playerPos = ctx.registry->GetComponent<PositionComponent>(playerID);
	if (!playerPos) {
		return CommandResult::Failure("You are nowhere.");
	}

	return ResolveAndIntent(client, ctx, playerID, playerPos, params, "use");
}

CommandResult InteractCommandHandler::HandleOpen(ClientConnection* client, const std::vector<std::string>& params, GameContext& ctx)
{
	if (params.empty()) {
		return CommandResult::Failure("Open what?");
	}

	EntityID playerID = client->playerEntityID;
	auto* playerPos = ctx.registry->GetComponent<PositionComponent>(playerID);
	if (!playerPos) {
		return CommandResult::Failure("You are nowhere.");
	}

	return ResolveAndIntent(client, ctx, playerID, playerPos, params, "open");
}

CommandResult InteractCommandHandler::HandlePray(ClientConnection* client, const std::vector<std::string>& params, GameContext& ctx)
{
	if (params.empty()) {
		return CommandResult::Failure("Pray at what?");
	}

	EntityID playerID = client->playerEntityID;
	auto* playerPos = ctx.registry->GetComponent<PositionComponent>(playerID);
	if (!playerPos) {
		return CommandResult::Failure("You are nowhere.");
	}

	return ResolveAndIntent(client, ctx, playerID, playerPos, params, "pray");
}

CommandResult InteractCommandHandler::ResolveAndIntent(ClientConnection* client, GameContext& ctx,
	EntityID playerID, PositionComponent* playerPos,
	const std::vector<std::string>& params, const std::string& action)
{
	(void)client;

	auto accepts = [&](InteractableComponent* inter) {
		if (action == "open") return inter->interaction_type == "door" || inter->interaction_type == "chest";
		if (action == "pray") return inter->interaction_type == "healing";
		return true;
	};

	for (EntityID e : ctx.entityFind->FindInEquipment(playerID, params)) {
		auto* inter = ctx.registry->GetComponent<InteractableComponent>(e);
		if (inter && accepts(inter)) {
			InteractableIntentComponent intent{ e, action, "" };
			ctx.registry->AddComponent<InteractableIntentComponent>(playerID, intent);
			return CommandResult::Success();
		}
	}

	for (EntityID e : ctx.entityFind->FindByShape(
			playerPos->roomId, playerPos->x, playerPos->y,
			params, EntityResolver::ShapeAround())) {
		auto* inter = ctx.registry->GetComponent<InteractableComponent>(e);
		if (!inter) continue;
		if (!accepts(inter)) continue;
		InteractableIntentComponent intent{ e, action, "" };
		ctx.registry->AddComponent<InteractableIntentComponent>(playerID, intent);
		return CommandResult::Success();
	}

	return CommandResult::Failure("You don't see that here.");
}
