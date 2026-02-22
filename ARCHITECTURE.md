# ModularMUD Server Architecture

## Overview

ModularMUD is a high-performance, data-oriented MUD (Multi-User Dungeon) game server built with modern C++17. It uses an Entity-Component-System (ECS) architecture for efficient game state management and supports multiple client types simultaneously through a hybrid networking layer.

## Key Features

- **ECS Architecture**: Cache-friendly component storage with O(1) access patterns
- **Hybrid Client Support**: Simultaneous Telnet and WebSocket/JSON client support
- **Data-Driven Design**: Game content defined in JSON and Lua files
- **Event-Driven Systems**: Decoupled system communication via EventBus
- **Scripting**: Lua integration for game logic and interactions
- **Persistent Storage**: SQLite database for player data

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                         GameEngine                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   Systems    │  │   Factories  │  │   Context    │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        ▼                     ▼                     ▼
┌──────────────┐      ┌──────────────┐      ┌──────────────┐
│   Registry   │      │   World      │      │  EventBus    │
│  (ECS Core)  │      │  (Game Map)  │      │  (Messaging) │
└──────────────┘      └──────────────┘      └──────────────┘
        │                     │                     │
        └─────────────────────┼─────────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     GameContext                             │
│  Central dependency container holding all game resources   │
└─────────────────────────────────────────────────────────────┘
```

---

## Entity-Component-System (ECS)

### Design Philosophy

The ECS architecture separates data (Components) from behavior (Systems), providing:

- **Cache Efficiency**: Components stored in contiguous memory
- **Flexibility**: Entities are just IDs, compositions change at runtime
- **Performance**: Systems iterate over relevant components only
- **Decoupling**: Systems don't know about each other

### Core Components

#### Registry
**File**: `Registry.h/cpp`

The central ECS manager that:
- Creates and destroys entities (simple integer IDs)
- Manages component storage via `ComponentPool<T>`
- Provides type-safe component access via templates
- Supports single-component views for iteration

```cpp
// Creating an entity
EntityID player = registry.CreateEntity();

// Adding components
registry.AddComponent<PositionComponent>(player, roomId, x, y);
registry.AddComponent<StatComponent>(player, 100, 100, 10, 10, 10, 10);

// Querying components
auto* pos = registry.GetComponent<PositionComponent>(player);
if (pos) {
    pos->x += 1;
}

// Iterating over all entities with a component
for (EntityID entity : registry.view<PositionComponent>()) {
    auto* pos = registry.GetComponent<PositionComponent>(entity);
    // Process position...
}
```

#### ComponentPool
**File**: `ComponentPool.h`

Template-based sparse-set storage:
- **Dense Array**: Contiguous component storage (cache-friendly iteration)
- **Sparse Array**: Maps EntityID to dense index (O(1) lookup)
- **Entity Array**: Tracks which entity owns each component

### Available Components

The game includes 51+ components organized by function:

**Identity & State**:
- `PlayerComponent` - Links entity to database account
- `MobComponent` - AI-controlled entity flag
- `NameComponent` - Display name
- `DescriptionComponent` - Entity description

**Position & Movement**:
- `PositionComponent` - Room ID and coordinates (x, y)
- `MoveIntentComponent` - Pending movement command
- `RegionComponent` - Current world region

**Combat & Stats**:
- `StatComponent` - Core stats (Health, Strength, etc.)
- `BaseStatComponent` - Base values before modifiers
- `StatModifierComponent` - Temporary stat changes
- `CombatIntentComponent` - Attack target and style
- `AttackIntentComponent` - Pending attack
- `BusyComponent` - Cooldown/recovery timer

**Inventory & Equipment**:
- `InventoryComponent` - List of item entities
- `EquipmentComponent` - Equipped items by slot
- `ItemComponent` - Item template reference
- `WeaponComponent` - Weapon stats
- `ArmourComponent` - Armor stats
- `PickupItemIntentComponent` - Pending item pickup
- `EquipItemIntentComponent` - Pending equip action

**Skills & Abilities**:
- `SkillHolderComponent` - Known skills
- `SkillDefinitionComponent` - Skill data
- `SkillIntentComponent` - Pending skill use
- `SkillWindupComponent` - Skill casting timer
- `CoolDownDefinitionComponent` - Cooldown configuration
- `ResourceCostComponent` - Mana/stamina costs

**AI & Behavior**:
- `BehaviourComponent` - AI type and state
- `AggressiveAIComponent` - Auto-attack flag

**Interaction**:
- `InteractableComponent` - Can be interacted with
- `InteractableIntentComponent` - Pending interaction
- `PortalComponent` - Teleport destination
- `ShrineComponent` - Blessing/shrine effects
- `LeverComponent` - Toggle state
- `ChestComponent` - Container storage

**Visual & UI**:
- `VisualComponent` - ASCII art representation
- `ClientComponent` - Network connection and message queue
- `SocketComponent` - Raw socket descriptor

**Events & Scripting**:
- `ScriptComponent` - Attached Lua scripts
- `ScheduledEventComponent` - Timed event triggers
- `PulseComponent` - Periodic update flag

**Progression**:
- `ProgressionComponent` - Experience and level
- `MasteryComponent` - Skill mastery levels
- `BodyComponent` - Mutations and modifications
- `RenownFactionComponent` - Faction standing
- `SetComponent` - Item set bonuses

**Loot**:
- `LootDropComponent` - Drop table reference
- `ValueComponent` - Gold value
- `WeightComponent` - Carry weight

---

## Game Systems

Systems process entities with specific component combinations each tick.

### System Execution Order

```
GameEngine::Update(float deltaTime)
├── movementSystem->MovementSystemRun()    // Process movements
├── interactionSystem->run()               // Handle interactions
├── networkSyncSystem->Run()               // Sync client views
├── invSystem->Run(deltaTime)              // Process inventory ops
├── combatSystem->run()                    // Resolve combat
├── updateSystem->Update(deltaTime)        // Update timers
├── eventBus->CallDefferedCalls()          // Process deferred events
├── cleanSystem->run()                     // Remove destroyed entities
└── saveSystem->Run(deltaTime)             // Periodic persistence
```

### Core Systems

#### CombatSystem
**File**: `CombatSystem.cpp`

Processes combat actions and damage calculation:
- Handles `AttackIntentComponent` and `CombatIntentComponent`
- Calculates damage based on stats, weapons, and attack styles
- Applies damage to target's `StatComponent`
- Manages recovery time via `BusyComponent`
- Generates combat messages for clients

**Key Features**:
- Attack style system (various combat stances)
- Damage type modifiers (physical, magical)
- Recovery time after attacks
- Death handling and defeat messages

#### MovementSystem
**File**: `MovementSystem.cpp`

Handles entity movement between rooms:
- Processes `MoveIntentComponent`
- Validates movement (check terrain, obstacles)
- Updates `PositionComponent`
- Triggers room entry events
- Notifies nearby clients of movement

#### NetworkSyncSystem
**File**: `NetworkSyncSystem.cpp`

Synchronizes game state with clients:
- Sends room descriptions to entering players
- Updates entity positions on client maps
- Broadcasts entity appearances/disappearances
- Handles client capability detection (web vs telnet)

#### InventorySystem
**File**: `InventorySystem.cpp`

Manages items and equipment:
- Processes equip/unequip intents
- Calculates stat modifiers from equipment
- Validates equipment slots and requirements
- Handles weight and capacity limits

#### InteractionSystem
**File**: `InteractionSystem.cpp`

Handles world interactions:
- Processes `InteractableIntentComponent`
- Executes Lua scripts for interactables
- Handles item pickup intents
- Teleport, spawn, and healing actions

#### SkillSystem
**File**: `SkillSystem.cpp`

Manages ability execution:
- Processes `SkillIntentComponent`
- Handles skill windup (casting time)
- Manages cooldowns
- Calculates resource costs
- Triggers skill effects

#### UpdateSystem
**File**: `UpdateSystem.cpp`

Updates time-based game state:
- Decrements `BusyComponent` timers
- Triggers `PulseComponent` events
- Processes `ScheduledEventComponent` triggers

#### MessageSystem
**File**: `MessageSystem.cpp`

Handles game messaging:
- Subscribes to game events
- Formats messages for different client types
- Broadcasts to rooms or globally

#### SaveSystem
**File**: `SaveSystem.cpp`

Manages persistent storage:
- Periodically saves dirty entities
- Tracks `VitalsChangedComponent`, `PositionChangedComponent`, etc.
- Delegates to `SQLiteDatabase` for actual storage

#### CleanUpSystem
**File**: `CleanUpSystem.cpp`

Removes destroyed entities:
- Finds entities with `DestroyTag`
- Safely destroys them after all systems have run
- Prevents iterator invalidation issues

#### BehaviorSystem
**File**: `BehaviorSystem.cpp`

Controls AI entities:
- Handles aggressive mob AI
- Reacts to player proximity
- Manages AI state transitions

---

## Factories

Factories create game entities from data templates.

### FactoryManager
**File**: `FactoryManager.h/cpp`

Central coordinator for all factories:
- Loads game data from JSON files
- Provides access to specialized factories
- Manages template caches

```cpp
gameContext.factories->LoadAllData();  // Load all JSON definitions
gameContext.factories->items.CreateItem("iron_sword", ...);
gameContext.factories->mobs.CreateMob("goblin", ...);
```

### Specialized Factories

#### ItemFactory
Creates items from Lua templates:
- Weapons with damage stats
- Armor with protection values
- Consumables with effects
- Container items

#### MobFactory
Creates AI-controlled entities:
- Stats from templates
- Behavior patterns
- Loot tables
- Spawn positions

#### PlayerFactory
Hydrates player entities from database:
- Loads stats from SQLite
- Restores inventory
- Re-equips items
- Sets position

#### InteractableFactory
Creates world objects:
- Doors, levers, chests
- Portals and teleporters
- Shrines and blessings
- Custom scripted objects

#### SkillFactory
Manages ability definitions:
- Skill effects and costs
- Cooldown configurations
- Windup/casting times
- Scaling formulas

#### LootFactory
Manages drop tables:
- Item drop chances
- Quantity ranges
- Rarity tiers

#### DialogueFactory
Manages NPC conversations:
- Dialogue trees
- Response options
- Condition checks

---

## Networking Architecture

### Hybrid Client Support

The server supports multiple client types simultaneously:

1. **Telnet Clients** - Traditional text-based MUD clients
2. **Web Clients** - Modern WebSocket/JSON clients
3. **GMCP Clients** - Mudlet and similar with GMCP support

### NetworkSystem
**File**: `NetworkSystem.cpp`

Central networking coordinator:
- Manages `ClientConnection` objects
- Handles message queuing
- Flushes messages at end of each tick
- Formats output for different client capabilities

### ClientComponent
**File**: `ClientComponent.h`

Per-player network state:
```cpp
struct ClientComponent {
    ClientConnection* client;           // Network connection
    std::vector<GameMessage> messageQueue;  // Pending messages
    bool isWebClient = false;           // Client capability flags
    bool hasSideBar = false;
    bool hasMiniMap = false;
};
```

### GameMessage Structure
```cpp
struct GameMessage {
    std::string type;           // "combat_hit", "room_enter", etc.
    std::string consoleText;    // Human-readable with ANSI codes
    std::string jsonData;       // Structured data for web clients
};
```

### Message Flow

1. **Game Logic** queues messages:
   ```cpp
   GameMessage msg("combat_hit", "You deal &R15&X damage!", jsonData);
   client->QueueGameMessage(msg);
   ```

2. **NetworkSystem::FlushQueues()** (end of tick):
   - For web clients: Send JSON with console_text + ui_data
   - For telnet: Send consoleText with ANSI codes
   - For GMCP: Send both text and GMCP payload

3. **Late Binding**: Client type determined at flush time, not message creation

---

## Event System

### EventBus
**File**: `EventBus.h`

Type-safe publish-subscribe messaging:
```cpp
// Define event types
enum class EventType {
    RoomEntered,
    CombatStarted,
    EntityDied,
    // ...
};

// Subscribe to events
eventBus->Subscribe(EventType::RoomEntered, [](EventContext& ctx) {
    auto& data = std::get<RoomEventData>(ctx.data);
    // Handle room entry...
});

// Publish events
eventBus->Publish(EventType::RoomEntered, eventContext);

// Deferred execution (end of tick)
eventBus->PublishDeferred(EventType::RoomEntered, eventContext);
```

### Event Context
```cpp
struct EventContext {
    std::variant<
        RoomEventData,
        CombatEventData,
        DeathEventData,
        // ...
    > data;
};
```

### Common Event Types

- **RoomEntered**: Entity enters a room
- **RoomExited**: Entity leaves a room
- **CombatStarted**: Combat engagement begins
- **CombatEnded**: Combat concludes
- **EntityDied**: Entity defeated
- **ItemPickedUp**: Item collected
- **ItemDropped**: Item dropped
- **SkillUsed**: Ability activated

---

## Scripting System

### ScriptManager
**File**: `ScriptManager.cpp`

Lua integration for game logic:
- Loads scripts from `scripts/` directory
- Exposes game API to Lua
- Handles script events
- Manages interactable callbacks

### ScriptEventBridge
**File**: `ScriptEventBridge.h`

Connects EventBus to Lua:
- Forwards C++ events to Lua handlers
- Allows scripts to subscribe to game events

### Lua API

Scripts have access to:
- Entity queries and manipulation
- Component reading/writing
- Event publishing
- Logging and debugging

---

## Data Persistence

### SQLiteDatabase
**File**: `SQLiteDatabase.cpp`

Manages player data storage:
- Player stats and progress
- Inventory items
- Equipment state
- Body modifications/mutations
- Password hashes and salts

### Schema

**players table**:
- `id` - Primary key
- `account_id` - PlayerComponent reference
- `name` - Username
- `password_hash` - SHA-256 hash
- `salt` - Password salt
- `room_id` - Current location
- `region_id` - Current region
- `stats` - JSON stat blob
- `body_mods` - JSON mutations

**player_items table**:
- `id` - Primary key
- `owner_id` - Foreign key to players
- `template_id` - Item template reference
- `item_state` - JSON (equipped, slot, etc.)

### Save Strategy

1. **Dirty Tracking**: Components marked as changed
2. **Periodic Save**: Every N seconds or on logout
3. **Selective Updates**: Only changed components saved

---

## World System

### World
**File**: `World.cpp`

Manages game world state:
- Room definitions and connections
- Entity spatial indexing
- Region management
- Terrain data

### WorldManager
**File**: `WorldManager.cpp`

High-level world operations:
- Teleportation
- Region transitions
- Room lookups
- Spatial queries

### Room
**File**: `Room.h`

Individual location data:
- Unique ID and name
- Description
- Exits (direction → room ID)
- Terrain type
- Entities present
- Items present

---

## Threading Model

### Architecture

```
Main Thread:
├── GameEngine::Update()        // Game logic
│   └── All systems run here
└── Render/Network flush

Network Thread:
├── Server::Run()               // Socket I/O
├── Accept connections
└── Queue client input
```

### Thread Safety

- **ThreadSafeQueue**: Lock-free input queue (client → game)
- **Registry**: Single-threaded access (main thread only)
- **ClientComponent**: Protected by message queue

### Input Flow

1. Network thread receives data
2. Parses into `ClientInput` struct
3. Pushes to `ThreadSafeQueue`
4. Main thread pops and processes each tick

---

## Game Loop

```cpp
// Main.cpp
const int TICKS_PER_SECOND = 30;
const int SKIP_TICKS = 1000 / TICKS_PER_SECOND;

while (engine.IsRunning()) {
    auto next_tick = GetTickCount64() + SKIP_TICKS;
    
    // 1. Process all queued inputs
    engine.ProcessInputs();
    
    // 2. Update game world (all systems)
    engine.Update(0.033f);
    
    // 3. Maintain tick rate
    int sleep_time = next_tick - GetTickCount64();
    if (sleep_time > 0) {
        Sleep(sleep_time);
    }
}
```

### Tick Timing

- **Target Rate**: 30 ticks/second (~33ms per tick)
- **Fixed Timestep**: Consistent for game logic
- **Sleep Compensation**: Prevents busy-waiting

---

## Project Structure

```
ModularMudServer/
├── Core Files
│   ├── Main.cpp                    # Entry point
│   ├── GameEngine.h/cpp           # Main game controller
│   ├── GameContext.h/cpp          # Dependency container
│   ├── Server.h/cpp               # Network server
│   └── ClientConnection.h/cpp     # Client session
│
├── ECS Core
│   ├── Registry.h/cpp             # Entity/component manager
│   ├── ComponentPool.h            # Component storage
│   ├── Component.h                # Component includes
│   └── Entity.h/cpp               # Entity definition
│
├── Systems/
│   ├── CombatSystem.h/cpp
│   ├── MovementSystem.h/cpp
│   ├── NetworkSystem.h/cpp
│   ├── NetworkSyncSystem.h/cpp
│   ├── InteractionSystem.h/cpp
│   ├── InventorySystem.h/cpp
│   ├── SkillSystem.h/cpp
│   ├── UpdateSystem.h/cpp
│   ├── BehaviorSystem.h/cpp
│   ├── MessageSystem.h/cpp
│   ├── SaveSystem.h/cpp
│   └── CleanUpSystem.h/cpp
│
├── Components/ (51 component headers)
│   ├── PositionComponent.h
│   ├── StatComponent.h
│   ├── ClientComponent.h
│   └── ... (48 more)
│
├── Factories/
│   ├── FactoryManager.h/cpp
│   ├── ItemFactory.h/cpp
│   ├── MobFactory.h/cpp
│   ├── PlayerFactory.h/cpp
│   ├── InteractableFactory.h/cpp
│   ├── SkillFactory.h
│   ├── LootFactory.h
│   └── DialogueFactory.h
│
├── World/
│   ├── World.h/cpp
│   ├── WorldManager.h/cpp
│   ├── Room.h/cpp
│   └── TerrainDef.h/cpp
│
├── Events/
│   ├── EventBus.h
│   └── ScriptEventBridge.h
│
├── Scripting/
│   ├── ScriptManager.h/cpp
│   └── scripts/                   # Lua scripts
│
├── Data/
│   ├── SQLiteDatabase.h/cpp
│   ├── PlayerData.h
│   └── *.json                     # Game data
│
├── States/
│   ├── LoginState.h
│   ├── CharCreatorState.h
│   ├── PlayingState.h
│   ├── MenuState.h
│   └── PasswordResetState.h
│
├── Utilities/
│   ├── ThreadSafeQueue.h
│   ├── TextHelperFunctions.h/cpp
│   ├── picosha2.h                 # SHA-256 hashing
│   └── IDatabase.h
│
└── Data Files
    ├── world_data.json           # World definition
    ├── items.json                # Item templates
    ├── mobs.json                 # Mob definitions
    ├── skills.json               # Skill definitions
    ├── interactables.json        # World objects
    ├── dialogue.json             # NPC dialogue
    ├── regions/floor1/           # Region data
    └── scripts/                  # Lua scripts
```

---

## Building and Running

### Prerequisites

- C++17 compatible compiler (MSVC, GCC, Clang)
- CMake 3.14+ (optional)
- vcpkg for dependencies

### Dependencies

- **nlohmann/json** - JSON parsing
- **sqlite3** - Database
- **sol2** - Lua bindings
- **lua** - Scripting language

### Build Steps

```bash
# Using vcpkg (Windows)
vcpkg install nlohmann-json sqlite3 sol2 lua

# Build with Visual Studio
# Open ModularMudServer.sln
# Build solution (x64/Debug or x64/Release)

# Or command line
msbuild ModularMudServer.sln /p:Configuration=Release
```

### Running

```bash
# Start server
./ModularMudServer.exe

# Server listens on port 27015
# Connect with telnet: telnet localhost 27015
# Or with web client via WebSocket
```

---

## Development Guidelines

### Adding a New Component

1. Create header file: `MyComponent.h`
2. Include in `Component.h`
3. Use in systems via Registry API

### Adding a New System

1. Create `MySystem.h/cpp`
2. Add to `GameEngine.h` as member
3. Initialize in `GameEngine` constructor
4. Call in `GameEngine::Update()`
5. Clean up in `GameEngine` destructor

### Adding a New Event

1. Add to `EventType` enum
2. Define event data struct
3. Add to `EventContext` variant
4. Publish from systems
5. Subscribe handlers via EventBus

### Data File Changes

- JSON files loaded at startup by FactoryManager
- Changes require server restart
- Invalid JSON will crash on load (add validation!)

---

## Performance Considerations

### Optimization Strategies

1. **Cache-Friendly ECS**: ComponentPool uses contiguous storage
2. **Deferred Events**: Prevents deep call stacks
3. **Dirty Tracking**: Only save changed data
4. **Single-Component Views**: Fast iteration over specific types
5. **Tick Rate**: 30Hz balances responsiveness and CPU usage

### Memory Management

- Systems allocated once in GameEngine constructor
- Entities destroyed via DestroyTag + CleanUpSystem
- SQLite connections managed via RAII
- Use unique_ptr for ownership, raw pointers for non-owning refs

---

## Security Notes

- Passwords hashed with SHA-256 + salt
- SQL injection prevented via prepared statements
- Buffer sizes checked (DEFAULT_BUFLEN = 1024)
- No direct client input executed as code

---

## Future Improvements

1. Multi-component views in Registry
2. Proper logging framework (spdlog)
3. Configuration file support
4. Unit tests (Catch2)
5. Hot-reload for scripts and data
6. Profiling and metrics
7. Web admin interface

---

## License

[Your License Here]

---

## Contributing

1. Follow existing code style
2. Add components to Component.h
3. Update ARCHITECTURE.md for major changes
4. Test with both Telnet and Web clients
5. Ensure clean compilation with /W4

---

*Last updated: 2026-02-18*
