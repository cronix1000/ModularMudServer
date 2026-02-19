#pragma once

#include "Command.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>

// Forward declarations only
struct GameContext;
class ClientConnection;
class GameEngine;

using CommandFunction = std::function<void(ClientConnection*, std::vector<std::string> input, const std::vector<int>& localIds)>;

class CommandInterpreter
{
public:
	GameContext& ctx;
	CommandInterpreter(GameContext& g);
	~CommandInterpreter();
	void Interpret(ClientConnection* client, Command* command);
	
	void RegisterCommands();
private:
	std::unordered_map<std::string, CommandFunction> core_command_map_;
	void HandleQuit(ClientConnection* client, std::vector<std::string> input);
	void HandleAttack(ClientConnection* client, std::vector<std::string> input);
	void HandleCast(ClientConnection* client, std::vector<std::string> input);
	void HandleMove(ClientConnection* client, std::vector<std::string> input);
	void HandlePickup(ClientConnection* client, std::vector<std::string> input);
	void HandleEquip(ClientConnection* client, std::vector<std::string> params);
	void HandleMenu(ClientConnection* client, std::vector<std::string> input);
	void HandleInteract(ClientConnection* client, std::vector<std::string> input);
	void HandleHello(ClientConnection* client, std::vector<std::string> input);
	
	// JSON Handshake handler for hybrid client detection
	bool TryHandleJSONHandshake(ClientConnection* client, const std::string& input);
};