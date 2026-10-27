#pragma once

#include "Platform.h"
#include "Command.h"
#include "TelnetCodec.h"
#include <string>
#include <queue>
#include <stack>
#include <unordered_set>
#include "ClientInput.h"
#include "ThreadSafeQueue.h"
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

	bool hasGMCP;
		TelnetState telnetState;
		telnet::Decoder telnetDecoder;
		std::unordered_set<std::string> subscribedModules;
	SocketType tcpSocket;
	int clientID;
	std::string recvBuffer;
	ClientConnection(SocketType newSocket) : tcpSocket(newSocket) {
		needsCleanup = false;
		hasGMCP = false;
		playerEntityID = -1;
		playerId = -1;

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
	void ProcessIncomingBytes(const uint8_t* data, size_t len);
	bool isSubscribed(const std::string module) const {
		return subscribedModules.empty() || subscribedModules.count(module) > 0;
	}
	void QueueMessage(const std::string& msg);
	void QueueOutbound(const OutboundPacket& packet);
	std::queue<std::string> OutboundMessages;
	ThreadSafeQueue<OutboundPacket> outbound;
	std::vector<std::string> DrainCompleteLines();
	void DisconnectGracefully();
	bool needsCleanup;
	std::stack<GameState*> stateStack;
	void PopState();
	void PushState(GameState* state);
	GameEngine* GetEngine() { return engine; }
	int playerEntityID;
	void SetEngine(GameEngine* _engine) { engine = _engine; }
	void SendPacket(const OutboundPacket& packet);

private:
	GameEngine* engine;

	std::string inputBuffer;
};
