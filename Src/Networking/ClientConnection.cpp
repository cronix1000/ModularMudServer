#include "ClientConnection.h"

#include "PlayerComponents.h"
#include "GameContext.h"
#include "GameEngine.h"
#include "GameState.h"
#include "Registry.h"
#include "ThreadSafeQueue.h"
#include <cstring>
#include <cstdio>
#include <sstream>

int ClientConnection::RecieveData() {
	char recvbuf[DEFAULT_BUFLEN];
	// Clear buffer not strictly necessary if we use the return value correctly,
	// but good for safety.
	memset(recvbuf, 0, DEFAULT_BUFLEN);

	int bytesReceived = recv(this->tcpSocket, recvbuf, DEFAULT_BUFLEN - 1, 0);

	if (bytesReceived > 0) {
		ProcessIncomingBytes(reinterpret_cast<uint8_t*>(recvbuf), bytesReceived);
		return bytesReceived;
	}
	return 0;

}
std::vector<std::string> ClientConnection::DrainCompleteLines() {
	std::vector<std::string> lines;
	size_t pos;
	while ((pos = inputBuffer.find('\n')) != std::string::npos) {
		std::string line = inputBuffer.substr(0, pos);
		inputBuffer.erase(0, pos + 1);
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (!line.empty()) lines.push_back(std::move(line));
	}
	return lines;
}

int ClientConnection::SendData() {

	int iSendResult = 0;
	char recvbuf[DEFAULT_BUFLEN] = { 0 };
	int recvbuflen = DEFAULT_BUFLEN;
	int iResult = 0;
	std::string hugePacket = "";
	OutboundPacket pkt;
	while (outbound.TryPop(&pkt).has_value()) {
		hugePacket += pkt.escapeTelnet ? telnet::escapeText(pkt.data) : pkt.data;
	}
	// 3. Send ONLY ONCE
	if (!hugePacket.empty()) {
		iSendResult = send(this->tcpSocket, hugePacket.c_str(), static_cast<int>(hugePacket.size()), 0);

		if (iSendResult == SOCKET_ERROR_VAL) {
			needsCleanup = true;
		}
	}

	return iSendResult;
}

void ClientConnection::QueueMessage(const std::string& msg) {
	outbound.Push({ msg, /*Escape*/ true });
}

void ClientConnection::QueueOutbound(const OutboundPacket& packet)
{
	outbound.Push(packet);
}

void ClientConnection::SendPacket(const OutboundPacket& packet) {
	// Send the packet immediately (for GMCP and other protocol packets)
	int iSendResult = send(this->tcpSocket, packet.data.c_str(), static_cast<int>(packet.data.size()), 0);

	if (iSendResult == SOCKET_ERROR_VAL) {
		printf("SendPacket failed with error: %d\n", GetSocketError());
		// Don't close socket here - let the main loop handle disconnection
	}
}

void ClientConnection::ProcessIncomingBytes(const uint8_t* data, size_t len) {
	if (data == nullptr || len == 0) return;

	auto events = telnetDecoder.feed(data, len);

	for (const auto& ev : events) {
		switch (ev.kind) {
		case telnet::Event::TextByte: {
			if (!ev.data.empty()) {
				inputBuffer.push_back(static_cast<char>(ev.data[0]));
			}
			break;
		}

		case telnet::Event::Negotiation: {
			if (ev.option == telnet::GMCP) {
				hasGMCP = true;
				break;
			}
			break;

		case telnet::Event::SubnegStart: {
			telnetState.inSubneg = true;
			telnetState.subnegOpt = ev.option;
			telnetState.subnegBuf.clear();
			break;
		}

		case telnet::Event::SubnegData: {
			if (telnetState.inSubneg) {
				telnetState.subnegBuf.insert(
					telnetState.subnegBuf.end(),
					ev.data.begin(), ev.data.end());
			}
			break;
		}

		case telnet::Event::SubnegEnd: {
			if (!telnetState.inSubneg) break;

			if (ev.option == telnet::GMCP) {
				std::string raw(ev.data.begin(), ev.data.end());
				auto sp = raw.find(' ');
				if (sp != std::string::npos) {
					std::string module = raw.substr(0, sp);
					std::string jsonText = raw.substr(sp + 1);
					if (!stateStack.empty()) {
						stateStack.top()->HandleGMCP(this, module, jsonText);
					}
				}
			}

			telnetState.inSubneg = false;
			telnetState.subnegBuf.clear();
			break;
		}
		}
		}
	}
}

	void ClientConnection::DisconnectGracefully() {
		// 1. Send the TCP shutdown signal (SD_SEND)
		shutdown(this->tcpSocket, SD_SEND);

		this->needsCleanup = true;
	}

	void ClientConnection::PopState() {
		if (stateStack.empty()) return;

		// 1. Clean up the current state
		GameState* oldState = stateStack.top();
		delete oldState; // This triggers the destructor of the popped state

		// 2. Remove it
		stateStack.pop();

		// 3. WAKE UP the state that is now on top
		if (!stateStack.empty()) {
			stateStack.top()->OnResume(this);
		}

	}

	void ClientConnection::PushState(GameState * state) {
		stateStack.push(state);

		state->OnEnter(this);
	}
