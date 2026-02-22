#pragma once
#include "ClientConnection.h"
#include <vector>
#include <string>

// Rich message structure that supports both terminal and web clients
struct GameMessage {
    std::string type;           // e.g., "combat_hit", "room_enter", "heal"
    std::string consoleText;    // Human-readable text with ANSI color codes (e.g., "You take &R5&X damage!")
    std::string jsonData;       // Raw JSON string for UI clients (e.g., "{\"damage\": 5, \"current_hp\": 45}")
    
    GameMessage() = default;
    GameMessage(const std::string& msgType, const std::string& text, const std::string& data = "{}")
        : type(msgType), consoleText(text), jsonData(data) {}
};

struct ClientComponent {
    ClientConnection* client;
    
    // Message queue for late-binding presentation layer
    std::vector<GameMessage> messageQueue;
    
    // Client capability flags for hybrid client support
    bool isWebClient = false;      // True if client is a modern web client (expects JSON)
    bool hasSideBar = false;       // True if client supports sidebar UI (e.g., Mudlet GMCP)
    bool hasMiniMap = false;       // True if client supports minimap display
    
    // Helper methods for message queue management
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
    
    // Helper to update capability flags from hello packet
    void SetCapabilities(bool web, bool sidebar, bool minimap) {
        isWebClient = web;
        hasSideBar = sidebar;
        hasMiniMap = minimap;
    }
};
