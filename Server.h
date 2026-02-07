#pragma once
#undef UNICODE

#define WIN32_LEAN_AND_MEAN


#include <vector>
#include "ClientConnection.h"
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "MainMenuState.h"
#include "Registry.h"
//#include <sol/sol.hpp>
class Server {
public:
	GameEngine* gameEngine;
	Server(GameEngine* engine);
	~Server();
	void Init();
	bool Start(const char* port);
	void Run();
	bool Stop();
	bool AcceptClient();
private:
	std::vector<ClientConnection*> activeClients;
	SOCKET ListenSocket;
};