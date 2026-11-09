#pragma once
#include <string>
#include <vector>

struct InteractableContext {
    int interactableID;
    int userID; // The entity using the interactable
    int roomID;
    int x, y; // Position where interaction occurs
    std::string action; // "use", "activate", "open", "close", etc.
    std::string parameters; // Optional parameters
};

struct InteractableResult {
    bool success = false;
    std::string actionType = "none"; // "teleport", "spawn_item", "send_message", "trigger_event", etc.
    std::string message = ""; // Message to send to user
    std::string roomMessage = ""; // Message to send to room
    
    // Teleport data
    int targetRoomID = -1;
    int targetX = -1;
    int targetY = -1;
    
    // Item spawning
    std::string spawnItemID = "";
    int spawnX = -1;
    int spawnY = -1;
    
    // Event triggering
    std::string eventName = "";
    std::vector<std::string> eventParams;
    
    // State changes
    std::string newState = ""; // For doors, switches, etc.
    bool consumeOnUse = false;
};