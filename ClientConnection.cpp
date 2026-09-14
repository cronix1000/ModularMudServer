#include "ClientConnection.h"

#include "ClientComponent.h"
#include "GameContext.h"
#include "GameEngine.h"
#include "GameState.h"
#include "Registry.h"
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

void ClientConnection::ProcessInput() {
	size_t pos = 0;

	// Find position of the next newline character
	while ((pos = inputBuffer.find('\n')) != std::string::npos) {

		// 1. Extract the command line (up to the \n)
		std::string line = inputBuffer.substr(0, pos);

		// 2. Remove the processed part from the buffer (including the \n at pos)
		inputBuffer.erase(0, pos + 1);

		// 3. Handle Telnet \r (Carriage Return) if present at the end
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}

		// 4. Skip processing if the line is empty (user just hit enter)
		if (line.empty()) continue;

		// 5. Tokenize (Split string by spaces into vector)
		std::vector<std::string> parameters;
		std::stringstream ss(line);
		std::string token;
		while (ss >> token) {
			parameters.push_back(token);
		}

		// 6. Safety Check: Ensure we actually have words before processing
		if (parameters.empty()) continue;

		// 8. Pass to the active Game State (Menu or Playing)
		if (!stateStack.empty()) {
			stateStack.top()->HandleInput(this, parameters);
		}
	}
}

int ClientConnection::SendData() {

	int iSendResult = 0;
	char recvbuf[DEFAULT_BUFLEN] = { 0 };
	int recvbuflen = DEFAULT_BUFLEN;
	int iResult = 0;
	std::string hugePacket = "";

	while (!OutboundMessages.empty()) {
		hugePacket += telnet::escapeText(OutboundMessages.front());
		OutboundMessages.pop();
	}

	// 3. Send ONLY ONCE
	if (!hugePacket.empty()) {
		iSendResult = send(this->tcpSocket, hugePacket.c_str(), static_cast<int>(hugePacket.size()), 0);

		if (iSendResult == SOCKET_ERROR_VAL) {
			printf("send failed with error: %d\n", GetSocketError());
			CloseSocket(this->tcpSocket);
			SocketCleanup();
			return iResult;
		}
		printf("Bytes sent: %d\n", iSendResult);
	}

	return iSendResult;
}

void ClientConnection::QueueMessage(const std::string& msg) {
	OutboundMessages.push(msg);
}

void ClientConnection::SendPacket(std::string packet) {
	// Send the packet immediately (for GMCP and other protocol packets)
	int iSendResult = send(this->tcpSocket, packet.c_str(), static_cast<int>(packet.size()), 0);

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
					if (engine == nullptr) break;
					if (playerEntityID == 0) break;

					auto* cc = engine->gameContext.registry->GetComponent<ClientComponent>(playerEntityID);
					if (cc != nullptr) {
						cc->hasGMCP = true;
					}
				}
				break;
			}

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

void ClientConnection::PushState(GameState* state) {
	stateStack.push(state);

	state->OnEnter(this);
}
