#include "GameEngine.h"
#include "Registry.h"
#include "World.h"
#include "WorldManager.h"
#include "MovementSystem.h"
#include "SQLiteDatabase.h"
#include "PlayerData.h"
#include "ClientConnection.h"
#include "NetworkSyncSystem.h"
#include "EventBus.h"
#include "NetworkSystem.h"
#include "ScriptManager.h"
#include "ScriptEventBridge.h"
#include "UpdateSystem.h"
#include "PlayerFactory.h"
#include "ItemFactory.h"
#include "MobFactory.h"
#include "GameContext.h"
#include "FactoryManager.h"
#include "BehaviorSystem.h"
#include "CombatSystem.h"
#include "CleanUpSystem.h"
#include "InventorySystem.h"
#include "InteractionSystem.h"
#include "CommandInterpreter.h"
#include "MessageSystem.h"
#include "SaveSystem.h"
#include "RespawnSystem.h"
#include "WorldClimateSystem.h"
#include "AmbientAISystem.h"
#include "TargetingSystem.h"
#include "CombatStateSystem.h"
#include "RegionComponent.h"
#include "TimeData.h"
#include "ClientInput.h"
#include "GameState.h"
#include "picosha2.h"
#include "CommandRegistry.h"
#include "CommandInitializer.h"
#include "SkillSystem.h"
#include <set>

GameEngine::GameEngine(GameContext& ctx, ThreadSafeQueue<ClientInput>& input) : gameContext(ctx), isRunning(true), inputQueue(input) {
    // 1. Initialize core resources
    world = new World();
    gameContext.registry = std::make_unique<Registry>();
    gameContext.eventBus = std::make_unique<EventBus>();
    gameContext.scripts = std::make_unique<ScriptManager>(*gameContext.registry);
    gameContext.worldManager = std::make_unique<WorldManager>(world, gameContext.registry.get());
    gameContext.scripts->init();
    gameContext.scripts->load_all_scripts("scripts");
    gameContext.scripts->lua.script("print('Hello from Lua')");
    scriptEventBridge = new ScriptEventBridge(gameContext.eventBus.get(), gameContext.scripts.get());
    gameContext.db = std::make_unique<SQLiteDatabase>();
	gameContext.db->Connect("mud.db");


    // 3. Link the manager back to the context

    gameContext.time = std::make_unique<TimeData>();
    gameContext.factories = std::make_unique<FactoryManager>(gameContext);
    gameContext.interpreter = std::make_unique<CommandInterpreter>(gameContext);
    
    // Initialize new command system
    gameContext.commandRegistry = std::make_unique<CommandRegistry>(gameContext, gameContext.scripts->lua);
    CommandInitializer::RegisterAllCommands(*gameContext.commandRegistry);
    
    // 3. initialize systems
    movementSystem = new MovementSystem(gameContext);
    networkSystem = new NetworkSystem(gameContext);
    networkSyncSystem = new NetworkSyncSystem(gameContext);
    invSystem = new InventorySystem(gameContext);
    behaviorSystem = new BehaviorSystem(gameContext);
    updateSystem = new UpdateSystem(gameContext);
    combatSystem = new CombatSystem(gameContext);
    skillSystem = new SkillSystem(gameContext);
    respawnSystem = new RespawnSystem(gameContext);
    gameContext.respawnSystem = respawnSystem;  // Make accessible via GameContext
    climateSystem = new WorldClimateSystem(gameContext);
    ambientAISystem = new AmbientAISystem(gameContext);
    interactionSystem = new InteractionSystem(gameContext);
    messageSytem = new MessageSystem(gameContext);
    saveSystem = new SaveSystem(gameContext);
    cleanSystem = new CleanUpSystem(gameContext);
    targetingSystem = new TargetingSystem(gameContext);
    combatStateSystem = new CombatStateSystem(gameContext);

    // Initialize climate zones for each loaded region
    // This will be called after regions are loaded
    InitializeClimateZones();

    // Add and global entity as 1
    gameContext.registry->CreateEntity();

    gameContext.factories->LoadAllData();

    //4. Initilise scripts that need to be run right away (e.g event listeners)
    messageSytem->SubscribeToEvents();
    networkSystem->SetupListeners();
    behaviorSystem->SetupListeners();

}

GameEngine::~GameEngine()
{
    // Clean up all systems allocated with new
    delete movementSystem;
    delete networkSystem;
    delete networkSyncSystem;
    delete invSystem;
    delete behaviorSystem;
    delete updateSystem;
    delete combatSystem;
    delete respawnSystem;
    delete climateSystem;
    delete ambientAISystem;
    delete interactionSystem;
    delete messageSytem;
    delete saveSystem;
    delete cleanSystem;
    delete scriptEventBridge;
    delete world;

    // Factories are managed by FactoryManager which is in GameContext
    // GameContext's unique_ptrs will be automatically cleaned up
}

std::string GenerateSalt(int length = 16) {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!@#$%^&*";

    // Setup random number generator securely
    std::default_random_engine rng(std::random_device{}());
    std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);

    std::string salt;
    for (int i = 0; i < length; ++i) {
        salt += charset[dist(rng)];
    }
    return salt;
}

int GameEngine::CreatePlayer(ClientConnection* clientID, std::string username, std::string password,PlayerData playerData) {
    std::string salt = GenerateSalt();
    std::string combined = password + salt;
    std::string hash = picosha2::hash256_hex_string(combined);

    int newID = gameContext.db->CreatePlayerRow(username, hash, salt);
    return newID;
}



int GameEngine::LoadPlayer(ClientConnection* socket, std::string username) {
    // The Factory handles checking the DB and attaching all components
    EntityID id = gameContext.factories->player.LoadPlayer(username, socket);

    if (id == -1) {
        printf("Failed to load or create player %s\n", username.c_str());
        return -1;
    }

    ClientComponent* client = gameContext.registry->GetComponent<ClientComponent>(id);
    if (client) {
        EventContext ectx;
        ectx.data = RoomEventData{ id, gameContext.registry->GetComponent<PositionComponent>(id)->roomId };
        gameContext.eventBus->Publish(EventType::RoomEntered, ectx);
        gameContext.registry->AddComponent<PositionChangedComponent>(id);
    }

    printf("Player %s logged in as Entity %d\n", username.c_str(), id);
    return id;
}

void GameEngine::Update(float deltaTime) {
    time += deltaTime;

    gameContext.time->deltaTime = deltaTime;
    gameContext.time->globalTime += (double)deltaTime;

    movementSystem->MovementSystemRun();
    targetingSystem->Run(deltaTime);        // Resolve ambiguous targets
    interactionSystem->run();
    networkSyncSystem->Run();
    invSystem->Run(deltaTime);
    behaviorSystem->Run(deltaTime);         // AI decision making
    combatStateSystem->Run(deltaTime);      // Auto-attack timer
    skillSystem->Run(deltaTime);
    combatSystem->run();
    updateSystem->Update(deltaTime);
    respawnSystem->Update(deltaTime);
    climateSystem->Update(deltaTime);
    ambientAISystem->Update(deltaTime);
    gameContext.eventBus->CallDefferedCalls();
    cleanSystem->run();
    saveSystem->Run(deltaTime);
    networkSystem->FlushQueues();
}

const bool GameEngine::IsRunning() { return isRunning; }

ClientConnection* GameEngine::GetClientById(int clientId) {
	auto view = gameContext.registry->view<ClientComponent>();
	for (EntityID entity : view) {
		ClientComponent* client = gameContext.registry->GetComponent<ClientComponent>(entity);
		if (client && client->client && client->client->clientID == clientId) {
            return client->client;
		}
	}
}

void GameEngine::ProcessInputs() {
    ClientInput input;

    // "TryPop" returns true if it got data, false if empty.
    // We loop until the queue is empty so we handle ALL commands 
    // that arrived since the last frame.
    while (inputQueue.TryPop(&input)) {

        // 1. Find which Entity belongs to this Client
        // (You likely have a helper or map for this)
        ClientConnection* client = GetClientById(input.clientID);

        std::vector<std::string> inputStringVector;
        std::stringstream ss(input.rawText);
        std::string token;
        while (ss >> token) {
            inputStringVector.push_back(token);
        }

        if (!inputStringVector.empty() && client && !client->stateStack.empty()) {
            client->stateStack.top()->HandleInput(client, inputStringVector);
        }
    }
}

// Allow the game to close itself (e.g., from a "shutdown" command)
void GameEngine::Quit() { isRunning = false; }

GameContext& GameEngine::GetGameContext()
{
    return gameContext;
}

void GameEngine::InitializeClimateZones() {
    // Get all unique regions from loaded rooms
    std::set<std::string> uniqueRegions;

    for (EntityID entity : gameContext.registry->view<RegionComponent>()) {
        auto* region = gameContext.registry->GetComponent<RegionComponent>(entity);
        if (region) {
            uniqueRegions.insert(region->region);
        }
    }

    // Create a climate zone for each unique region
    for (const auto& regionId : uniqueRegions) {
        // Skip if already exists
        if (climateSystem->GetZoneClimate(regionId)) {
            continue;
        }

        // Create climate zone with default settings
        // You can customize based on region ID (e.g., floor1, floor2, dungeon, etc.)
        ZoneClimateComponent::ClimateType climateType = ZoneClimateComponent::ClimateType::TEMPERATE;
        bool isOutdoor = true;

        // Customize based on region name patterns
        if (regionId.find("dungeon") != std::string::npos ||
            regionId.find("cave") != std::string::npos ||
            regionId.find("underground") != std::string::npos) {
            climateType = ZoneClimateComponent::ClimateType::UNDERGROUND;
            isOutdoor = false;
        }
        else if (regionId.find("desert") != std::string::npos ||
                 regionId.find("arid") != std::string::npos) {
            climateType = ZoneClimateComponent::ClimateType::ARID;
        }
        else if (regionId.find("snow") != std::string::npos ||
                 regionId.find("ice") != std::string::npos ||
                 regionId.find("arctic") != std::string::npos) {
            climateType = ZoneClimateComponent::ClimateType::ARCTIC;
        }
        else if (regionId.find("tropical") != std::string::npos ||
                 regionId.find("jungle") != std::string::npos) {
            climateType = ZoneClimateComponent::ClimateType::TROPICAL;
        }
        else if (regionId.find("magic") != std::string::npos ||
                 regionId.find("ethereal") != std::string::npos) {
            climateType = ZoneClimateComponent::ClimateType::MAGICAL;
        }

        climateSystem->CreateClimateZone(regionId, regionId, climateType, isOutdoor);
    }
}

void GameEngine::OnPlayerChangedZone(int playerEntityId, const std::string& oldZone, const std::string& newZone) {
    // Notify climate system of zone changes
    if (!oldZone.empty()) {
        climateSystem->OnPlayerExitedZone(oldZone);
        ambientAISystem->OnPlayerExitedZone(oldZone);
    }

    if (!newZone.empty()) {
        climateSystem->OnPlayerEnteredZone(newZone);
        ambientAISystem->OnPlayerEnteredZone(newZone);
    }
}
