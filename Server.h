#pragma once

#include "Platform.h"
#include <vector>
#include "ClientConnection.h"
#include "ThreadSafeQueue.h"  
#include <unordered_map>

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
	void Run(std::stop_token stop_token);
	bool Stop();
	bool AcceptClient();
	std::shared_ptr<ClientConnection> GetClient(int clientId);
private:
	std::unordered_map<int,std::shared_ptr<ClientConnection>> activeClients;
	SocketType ListenSocket;
};