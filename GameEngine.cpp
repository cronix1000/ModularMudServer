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
#include "TimeData.h"

GameEngine::GameEngine(SQLiteDatabase* db) : db(db) {
    // 1. Initialize core resources
    registry =  new Registry();
    eventBus = new EventBus();
    world = new World();
    worldManager = new WorldManager(world);
    scriptManager = new ScriptManager(*registry);
    scriptManager->init();
    
    scriptEventBridge = new ScriptEventBridge(eventBus, scriptManager);

    gameContext = new GameContext{ *registry, *eventBus, *worldManager, *scriptManager, *db };

    // 3. Link the manager back to the context
    
    gameContext->time = std::make_unique<TimeData>();
    gameContext->factories = std::make_unique<FactoryManager>(*gameContext);
    gameContext->interpreter = std::make_unique<CommandInterpreter>(*gameContext);
    // 3. initialize systems 
    movementSystem = new MovementSystem(*gameContext);
    networkSystem = new NetworkSystem(*gameContext);
    networkSyncSystem = new NetworkSyncSystem(*gameContext);
    invSystem = new InventorySystem(*gameContext);
    behaviorSystem = new BehaviorSystem(*gameContext);
    updateSystem = new UpdateSystem(*gameContext);
    combatSystem = new CombatSystem(*gameContext);
    interactionSystem = new InteractionSystem(*gameContext);
    messageSytem = new MessageSystem(*gameContext);
    saveSystem = new SaveSystem(*gameContext);

    // Add and global entity as 1
    registry->CreateEntity();

    gameContext->factories->LoadAllData();

    //4. Initilise scripts that need to be run right away (e.g event listeners)
    //world->LoadWorld("world_data.json", *gameContext);
    messageSytem->SubscribeToEvents();
    networkSystem->SetupListeners();
    behaviorSystem->SetupListeners();
    scriptManager->load_all_scripts("scripts");
    scriptManager->lua.script("print('Hello from Lua')");
}

GameEngine::~GameEngine()
{
}


int GameEngine::CreatePlayer(ClientConnection* clientID, std::string username, PlayerData playerData) {
    int newID = db->CreatePlayerRow(username);
    return newID;
}

int GameEngine::LoadPlayer(ClientConnection* socket, std::string username) {
    // The Factory handles checking the DB and attaching all components
    EntityID id = gameContext->factories->player.LoadPlayer(username, socket);

    if (id == -1) {
        printf("Failed to load or create player %s\n", username.c_str());
        return -1;
    }

    ClientComponent* client = registry->GetComponent<ClientComponent>(id);
    if (client) {
        EventContext ectx;
        ectx.data = RoomEventData{ id, registry->GetComponent<PositionComponent>(id)->roomId };
        eventBus->Publish(EventType::RoomEntered, ectx);
        registry->AddComponent<PositionChangedComponent>(id);
    }

    printf("Player %s logged in as Entity %d\n", username.c_str(), id);
    return id;
}

void GameEngine::Update(float deltaTime) {
    time += deltaTime;

    gameContext->time->deltaTime = deltaTime;
    gameContext->time->globalTime += (double)deltaTime;

    movementSystem->MovementSystemRun();
    interactionSystem->run();
    networkSyncSystem->Run();
    invSystem->Run(deltaTime);
    combatSystem->run();
    updateSystem->Update(deltaTime);
    eventBus->CallDefferedCalls();
    saveSystem->Run(deltaTime);
}

GameContext& GameEngine::GetGameContext()
{
    return *gameContext;
}
