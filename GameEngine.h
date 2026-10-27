#pragma once
#include <iostream>
#include <unordered_map>
#include <ctime>
#include <string>
#include "ThreadSafeQueue.h"
#include <memory>

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
class CraftSystem;
class CleanUpSystem;
class InventorySystem;
class BehaviorSystem;
class InteractionSystem;
class TargetingSystem;
class CombatStateSystem;
class CommandInterpreter;
class MessageSystem;
class SaveSystem;
class RespawnSystem;
class WorldClimateSystem;
class AmbientAISystem;
struct TimeData;
class CommandRegistry;
struct ClientInput;
struct OutboundPacket;
class SkillSystem;
class EntityResolver;
class GameEngine
{
public:
	GameEngine(GameContext& ctx, ThreadSafeQueue<ClientInput>& inputQueue);
	~GameEngine();

	ThreadSafeQueue<ClientInput>& inputQueue;	
	std::unordered_map<int, std::shared_ptr<ClientConnection>> sessions;
	// Pass the address (&db)

	GameContext& gameContext;
	std::unique_ptr<World> world;
	std::unique_ptr <ScriptEventBridge> scriptEventBridge;

	float time = 0;

	GameContext& GetContext() { return gameContext; }
	int CreatePlayer(ClientConnection* clientID, std::string username, std::string password,PlayerData playerData);
	int LoadPlayer(ClientConnection* socket, std::string username);
	void Update(float deltaTime);
	const bool IsRunning();
	std::shared_ptr<ClientConnection> GetClientById(int clientId);
	int GetEntityByClient(int clientId);
	void ProcessInputs();
	void HandleDisconnect(int cliendItd);
	void SaveAllPlayers();
	void Quit();
	GameContext& GetGameContext();
	CommandInterpreter* interpreter;

	// Climate/Zone initialization
	void InitializeClimateZones();
	void OnPlayerChangedZone(int playerEntityId, const std::string& oldZone, const std::string& newZone);

	// SYSTEMS
	std::unique_ptr<MovementSystem> movementSystem;
	std::unique_ptr<NetworkSyncSystem> networkSyncSystem;
	std::unique_ptr < NetworkSystem>networkSystem;
	std::unique_ptr<EventBus> eventBus;
	std::unique_ptr<UpdateSystem>updateSystem;
	std::unique_ptr<InventorySystem> invSystem;
	std::unique_ptr<CleanUpSystem> cleanSystem;
	std::unique_ptr<BehaviorSystem> behaviorSystem;
	std::unique_ptr<CombatSystem> combatSystem;
	std::unique_ptr<InteractionSystem> interactionSystem;
	std::unique_ptr<MessageSystem> messageSytem;
	std::unique_ptr<SaveSystem> saveSystem;
	std::unique_ptr<RespawnSystem> respawnSystem;
	std::unique_ptr<WorldClimateSystem> climateSystem;
	std::unique_ptr<AmbientAISystem> ambientAISystem;
	std::unique_ptr<SkillSystem> skillSystem;
	std::unique_ptr<TargetingSystem> targetingSystem;
	std::unique_ptr<CombatStateSystem> combatStateSystem;
	std::unique_ptr<CraftSystem> craftSystem;

private:
	std::atomic<bool> isRunning = true;
};