#pragma once
#include <memory>

// Forward declarations
class EventBus;
class SQLiteDatabase;
class WorldManager;
class ScriptManager;
class Registry;
class FactoryManager;
class CommandInterpreter;
struct TimeData;

struct GameContext {
    Registry& registry;
    EventBus& eventBus;
    WorldManager& worldManager;
    ScriptManager& scripts;
    SQLiteDatabase& db;
    std::unique_ptr<TimeData> time;
    std::unique_ptr<FactoryManager> factories;
    std::unique_ptr<CommandInterpreter> interpreter;
};