#pragma once

#include "ClientConnection.h"

#include <string>
#include <vector>

// Rich message structure that supports both terminal and web clients
struct GameMessage {
	std::string type;        
	std::string consoleText;  
	std::string jsonData;    

	GameMessage() = default;
	GameMessage(const std::string& msgType, const std::string& text, const std::string& data = "{}")
		: type(msgType), consoleText(text), jsonData(data) {}
};

struct ClientComponent {
	ClientConnection* client;

	std::vector<GameMessage> messageQueue;

	bool isWebClient = false;     
	bool hasGMCP = false;          
	bool hasSideBar = false;     
	bool hasMiniMap = false;     

	void QueueGameMessage(const GameMessage& msg) {
		messageQueue.push_back(msg);
	}

	void QueueGameMessage(const std::string& type, const std::string& consoleText, const std::string& jsonData = "{}") {
		messageQueue.emplace_back(type, consoleText, jsonData);
	}

	bool HasPendingMessages() const {
		return !messageQueue.empty();
	}

	void ClearMessageQueue() {
		messageQueue.clear();
	}

	void SetCapabilities(bool web, bool gmcp, bool sidebar, bool minimap) {
		isWebClient = web;
		hasGMCP = gmcp;
		hasSideBar = sidebar;
		hasMiniMap = minimap;
	}
};