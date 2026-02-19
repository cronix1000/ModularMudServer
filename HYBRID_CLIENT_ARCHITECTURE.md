# Hybrid Client Architecture Implementation Summary

## Overview
Successfully refactored the ECS MUD engine to support a "Hybrid Client" architecture, enabling simultaneous support for modern Web Clients (via WebSockets/JSON) and legacy Telnet clients.

## Key Components Implemented

### 1. ClientComponent.h
**Location:** `ClientComponent.h`

Added late-binding message queue architecture:

```cpp
struct GameMessage {
    std::string type;           // e.g., "combat_hit", "room_enter"
    std::string consoleText;    // Human-readable text with ANSI codes
    json data;                  // Raw structured data for UI rendering
};

struct ClientComponent {
    ClientConnection* client;
    std::vector<GameMessage> messageQueue;
    
    // Client capability flags
    bool isWebClient = false;
    bool hasSideBar = false;
    bool hasMiniMap = false;
    
    void QueueGameMessage(const GameMessage& msg);
    void ClearMessageQueue();
    bool HasPendingMessages() const;
};
```

### 2. NetworkSystem (FlushQueues)
**Location:** `NetworkSystem.h/cpp`

Implemented late-binding message serialization:

```cpp
void NetworkSystem::FlushQueues() {
    // Called at end of every tick in GameEngine::Update()
    for each client with messages {
        if (client->isWebClient) {
            // Send JSON: {"type": "combat_hit", "console_text": "...", "ui_data": {...}}
        } else {
            // Send consoleText with ANSI parsing
            // Optionally send GMCP for Mudlet clients
        }
    }
}
```

**Web Client Output:**
```json
{
  "type": "combat_hit",
  "console_text": "You attack Goblin for &R15&X damage!",
  "ui_data": {
    "action": "attack",
    "damage": 15,
    "damage_type": "physical",
    "current_hp": 85,
    "max_hp": 100
  }
}
```

**Telnet Client Output:**
```
You attack Goblin for [RED]15[RESET] damage!
```

**GMCP Output (for Mudlet):**
```
IAC SB GMCP GameMessages.combat_hit {"damage":15,...} IAC SE
```

### 3. CombatSystem Refactoring
**Location:** `CombatSystem.cpp`

Refactored all combat actions to use GameMessage queue:

**Before:**
```cpp
sourceClient->client->QueueMessage("You attack for 5 damage!");
```

**After:**
```cpp
GameMessage msg;
msg.type = "combat_hit";
msg.consoleText = "You attack for &R5&X damage!";
msg.data = {
    {"damage", 5},
    {"damage_type", damageType},
    {"current_hp", targetStats->Health},
    {"max_hp", targetStats->MaxHealth}
};
sourceClient->QueueGameMessage(msg);
```

### 4. JSON Handshake Handler
**Location:** `CommandInterpreter.h/cpp`

Implemented client capability detection:

```cpp
bool TryHandleJSONHandshake(ClientConnection* client, const std::string& input) {
    // Parse JSON: {"type": "hello", "features": ["sidebar", "minimap"]}
    // Sets flags on ClientComponent
    // Responds with: {"type": "hello_ack", "server": "ModularMudServer", ...}
}
```

### 5. Updated Systems
All game systems now use the new architecture:

- **CombatSystem.cpp** - Combat messages (attack, heal, buff)
- **MessageSystem.cpp** - System and broadcast messages
- **NetworkSyncSystem.cpp** - Room display and map data
- **MovementSystem.cpp** - Movement failure messages
- **InteractionSystem.cpp** - Interaction and pickup messages
- **ScriptManager.cpp** - Lua script messages

### 6. GameEngine Integration
**Location:** `GameEngine.cpp`

Added `FlushQueues()` call at end of update loop:

```cpp
void GameEngine::Update(float deltaTime) {
    // ... existing system updates ...
    combatSystem->run();
    updateSystem->Update(deltaTime);
    
    // New: Flush all queued messages to clients
    networkSystem->FlushQueues();
}
```

## Message Types Supported

### Combat Messages
- `combat_hit` - Damage dealt/received
- `combat_heal` - Healing done/received
- `combat_buff` - Buff/debuff application
- `player_defeat` - Player death

### System Messages
- `system_message` - General system notifications
- `room_broadcast` - Messages to all in room
- `global_broadcast` - Server-wide announcements

### Interaction Messages
- `item_pickup` - Item collected
- `inventory_full` - Inventory capacity reached
- `interaction_success` - Successful interaction
- `interaction_failed` - Failed interaction

### Movement Messages
- `room_enter` - Player enters room
- `room_look` - Room display update
- `look_update` - Visual refresh
- `movement_failed` - Blocked movement

### Script Messages
- `script_message` - Messages from Lua scripts

## Client Protocol Examples

### Web Client (Nuxt/WebSocket)
```javascript
// Handshake
socket.send(JSON.stringify({
  type: "hello",
  features: ["sidebar", "minimap"]
}));

// Receive message
socket.onmessage = (event) => {
  const msg = JSON.parse(event.data);
  console.log(msg.console_text);  // Display in terminal
  updateUI(msg.ui_data);          // Update sidebar/minimap
};
```

### Telnet Client (PuTTY/Mudlet)
```
# No handshake needed - defaults to terminal mode
# Messages appear as:
You attack Goblin for 15 damage!

# Mudlet with GMCP support gets additional data:
# IAC SB GMCP GameMessages.combat_hit {"damage":15} IAC SE
```

## Architecture Benefits

1. **Late Binding** - Game logic doesn't know client type until flush time
2. **Single Code Path** - One implementation for all client types
3. **Rich UI Support** - Web clients get structured data for modern UIs
4. **Backward Compatible** - Telnet clients work without changes
5. **Extensible** - Easy to add new message types and client capabilities

## Migration Pattern

To migrate existing systems:

```cpp
// OLD: Direct sending
client->client->QueueMessage("Your message");

// NEW: Queue for late binding
GameMessage msg;
msg.type = "your_message_type";
msg.consoleText = "Your message";
msg.data = {{"key", value}};  // Optional: structured data
client->QueueGameMessage(msg);
```

## Files Modified

1. **ClientComponent.h** - Core data structures
2. **NetworkSystem.h/cpp** - FlushQueues implementation
3. **NetworkSyncSystem.h/cpp** - Room display messages
4. **CombatSystem.cpp** - Combat messages
5. **CommandInterpreter.h/cpp** - JSON handshake
6. **MessageSystem.cpp** - System messages
7. **MovementSystem.cpp** - Movement messages
8. **InteractionSystem.cpp** - Interaction messages
9. **ScriptManager.cpp** - Lua messages
10. **GameEngine.cpp** - FlushQueues integration

## Build Requirements

- C++17 or higher
- nlohmann/json library (via vcpkg)
- Existing ECS framework
- Winsock2 (Windows) or BSD sockets (Linux)

## Notes

- The implementation maintains full backward compatibility with existing Telnet clients
- Web clients must send the handshake packet before receiving structured data
- The FlushQueues() method is called at the end of every game tick
- ANSI color codes in consoleText are automatically converted for terminal clients
