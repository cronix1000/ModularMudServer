#pragma once

#include "CommandTrie.h"

// Forward declarations
class ClientConnection;
struct GameContext;
class CommandRegistry;

class SocialCommandHandler {
public:
	static void RegisterAll(CommandRegistry& registry);
	
private:
	static CommandResult HandleSay(ClientConnection* client,
								   const std::vector<std::string>& params,
								   GameContext& ctx);
	
	static CommandResult HandleTell(ClientConnection* client,
									const std::vector<std::string>& params,
									GameContext& ctx);
	
	static CommandResult HandleShout(ClientConnection* client,
									 const std::vector<std::string>& params,
									 GameContext& ctx);
	
	static CommandResult HandleEmote(ClientConnection* client,
									 const std::vector<std::string>& params,
									 GameContext& ctx);
	
	static CommandResult HandleWho(ClientConnection* client,
								   const std::vector<std::string>& params,
								   GameContext& ctx);
	
	// Helper to join parameters into message
	static std::string BuildMessage(const std::vector<std::string>& params);
};
