#pragma once

#include "Platform.h"
#include "Command.h"
#include <string>
#include <queue>
#include <stack>

class GameState;
class GameEngine;
class CommandInterpreter;

class ClientConnection
{
public:
	SocketType tcpSocket;
	int clientID;
	std::string recvBuffer;
	ClientConnection(SocketType newSocket) : tcpSocket(newSocket) {
	
	}
	~ClientConnection() {
		if (tcpSocket != INVALID_SOCKET_VAL) {
			CloseSocket(tcpSocket);
		}
	}
	int playerId;
	int RecieveData();
	void ProcessInput();
	int SendData();
	void SendPacket(std::string packet);
	void QueueMessage(const std::string& msg);
	std::queue<std::string> OutboundMessages;
	void DisconnectGracefully();
	bool needsCleanup;
	CommandInterpreter* commandInterpreter;
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
