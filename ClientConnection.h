#pragma once

#include "Platform.h"
#include "Command.h"
#include "TelnetCodec.h"
#include <string>
#include <queue>
#include <stack>
#include <unordered_set>

class GameState;
class GameEngine;

struct TelnetState {
	bool inSubneg = false;
	uint8_t subnegOpt = 0;
	std::vector<uint8_t> subnegBuf;
};

class ClientConnection
{
public:

		TelnetState telnetState;
		telnet::Decoder telnetDecoder;
		std::unordered_set<std::string> subscribedModules;
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
	void ProcessIncomingBytes(const uint8_t* data, size_t len);
	static std::string EscapeIAC(const std::string& text);
	bool isSubscribed(const std::string module) const {
		return subscribedModules.empty() || subscribedModules.count(module) > 0;
	}
	void QueueMessage(const std::string& msg);
	std::queue<std::string> OutboundMessages;
	void DisconnectGracefully();
	bool needsCleanup;
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
