#pragma once
#include <iostream>
#include <ctime>
#include <string>

// Minimal includes - only what's absolutely necessary
struct PlayerData;

// Forward declarations
class Registry;
class World;
class WorldManager;
class MovementSystem;
class SQLiteDatabase;
class ClientConnection;
class NetworkSyncSystem;
class EventBus;
class NetworkSystem;
class ScriptManager;
class ScriptEventBridge;
class UpdateSystem;
class PlayerFactory;
class ItemFactory;
class MobFactory;
struct GameContext;
class FactoryManager;
class CombatSystem;
class CleanUpSystem;
class InventorySystem;
class BehaviorSystem;
class InteractionSystem;
class CommandInterpreter;
class MessageSystem;
class SaveSystem;
struct TimeData;
class GameEngine
{
public:
	GameEngine(SQLiteDatabase* db);
	~GameEngine();

	GameContext* gameContext;
	Registry* registry;
	World* world;
	WorldManager* worldManager;
	ScriptManager* scriptManager;
	FactoryManager* factoryManager;
	ScriptEventBridge* scriptEventBridge;
	float time = 0;

	GameContext& GetContext() { return *gameContext; }
	int CreatePlayer(ClientConnection* clientID, std::string username, PlayerData playerData);
	int LoadPlayer(ClientConnection* socket, std::string username);
	void Update(float deltaTime);
	GameContext& GetGameContext();
	SQLiteDatabase* db;
	CommandInterpreter* interpreter;

	// Factories
	ItemFactory* itemFactory;
	MobFactory* mobFactory;
	PlayerFactory* playerFactory;

	// SYSTEMS
	MovementSystem* movementSystem;
	NetworkSyncSystem* networkSyncSystem;
	NetworkSystem* networkSystem;
	EventBus* eventBus;
	UpdateSystem* updateSystem;
	InventorySystem* invSystem;
	CleanUpSystem* cleanSystem;
	BehaviorSystem* behaviorSystem;
	CombatSystem* combatSystem;
	InteractionSystem* interactionSystem;
	MessageSystem* messageSytem;
	SaveSystem* saveSystem;
};