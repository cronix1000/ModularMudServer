#pragma once

#include <WinSock2.h>
#include "Command.h"
#include <string>
#include <queue>
#include <stack>

#define DEFAULT_BUFLEN 1024

class GameState;
class GameEngine;
class CommandInterpreter;
class ClientConnection
{
public:
	SOCKET tcpSocket;
	ClientConnection(SOCKET newSocket) : tcpSocket(newSocket) {
	
	}
	~ClientConnection() {
		if (tcpSocket != INVALID_SOCKET) {
			closesocket(tcpSocket);
		}
	}
	int playerId;
	int RecieveData();
	int SendData();
	void SendPacket(std::string packet);
	void QueueMessage(const std::string& msg);
	std::queue<std::string> OutboundMessages;
	void DisconnectGracefully();
	bool needsCleanup;
	CommandInterpreter* commandInterpretter;
	std::stack<GameState*> stateStack;
	void PopState();
	void PushState(GameState* state);
	GameEngine* GetEngine() { return engine; }
	int playerEntityID;
	void SetEngine(GameEngine* _engine) { engine = _engine; }
private:
	GameEngine* engine;
	std::string inputBuffer;
};