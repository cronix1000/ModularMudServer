#pragma once

#include "Platform.h"
#include <vector>
#include "ClientConnection.h"
#include "ThreadSafeQueue.h"  // Include the full template

struct GameContext;
struct GameContext;
struct ClientInput;
class GameEngine;

//#include <sol/sol.hpp>
class Server {
public:
	GameContext& gameContext;
	GameEngine* engine;
	ThreadSafeQueue<ClientInput>& inputQueue; // Change to reference

	Server(GameContext& context, GameEngine* engine, ThreadSafeQueue<ClientInput>& inputqueue);
	~Server();
	void Init();
	bool Start(const char* port);
	void HandleReceive(int clientID, std::string& buffer);
	void Run();
	bool Stop();
	bool AcceptClient();
private:
	std::vector<ClientConnection*> activeClients;
	SOCKET ListenSocket;
};