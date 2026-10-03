#include "CommandInitializer.h"
#include "CommandRegistry.h"
#include "CombatCommandHandler.h"
#include "MovementCommandHandler.h"
#include "ItemCommandHandler.h"
#include "SocialCommandHandler.h"
#include "LookCommandHandler.h"

void CommandInitializer::RegisterAllCommands(CommandRegistry& registry) {
	CombatCommandHandler::RegisterAll(registry);
	MovementCommandHandler::RegisterAll(registry);
	ItemCommandHandler::RegisterAll(registry);
	SocialCommandHandler::RegisterAll(registry);
	LookCommandHandler::RegisterAll(registry);
}
