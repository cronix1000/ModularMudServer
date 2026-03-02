#include "CommandInitializer.h"
#include "CommandRegistry.h"
#include "CombatCommandHandler.h"
#include "MovementCommandHandler.h"
#include "ItemCommandHandler.h"
#include "SocialCommandHandler.h"

void CommandInitializer::RegisterAllCommands(CommandRegistry& registry) {
	// Register all command handlers
	CombatCommandHandler::RegisterAll(registry);
	MovementCommandHandler::RegisterAll(registry);
	ItemCommandHandler::RegisterAll(registry);
	SocialCommandHandler::RegisterAll(registry);
}
