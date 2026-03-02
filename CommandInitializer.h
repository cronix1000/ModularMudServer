#pragma once

class CommandRegistry;

class CommandInitializer {
public:
	// Registers all built-in command handlers
	static void RegisterAllCommands(CommandRegistry& registry);
};
