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
    std::unique_ptr<Registry> registry;
    std::unique_ptr<EventBus> eventBus;
    std::unique_ptr<WorldManager> worldManager;
    std::unique_ptr <ScriptManager> scripts;
    std::unique_ptr <SQLiteDatabase> db;
    std::unique_ptr<TimeData> time;
    std::unique_ptr<FactoryManager> factories;
    std::unique_ptr<CommandInterpreter> interpreter;

    ~GameContext();
    GameContext();
};