#pragma once
#include <memory>

// Forward declarations
class EventBus;
class IDatabase;
class WorldManager;
class ScriptManager;
class Registry;
class FactoryManager;
class RespawnSystem;
class CommandRegistry;
class EntityResolver;
struct TimeData;

struct GameContext {
    std::unique_ptr<Registry> registry;
    std::unique_ptr<EventBus> eventBus;
    std::unique_ptr<WorldManager> worldManager;
    std::unique_ptr <ScriptManager> scripts;
    std::unique_ptr <IDatabase> db;
    std::unique_ptr<TimeData> time;
    std::unique_ptr<FactoryManager> factories;
    std::unique_ptr<CommandRegistry> commandRegistry;
    std::unique_ptr<EntityResolver> entityFind;
    RespawnSystem* respawnSystem;  // Not owned by GameContext, just a pointer

    ~GameContext();
    GameContext();
};