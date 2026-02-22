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
#include "ClientInput.h"
#include "GameState.h"
#include "picosha2.h"

GameEngine::GameEngine(GameContext& ctx, ThreadSafeQueue<ClientInput>& input) : gameContext(ctx), isRunning(true), inputQueue(input) {
    // 1. Initialize core resources
    world = new World();
    gameContext.registry = std::make_unique<Registry>();
    gameContext.eventBus = std::make_unique<EventBus>();
    gameContext.scripts = std::make_unique<ScriptManager>(*gameContext.registry);
    gameContext.worldManager = std::make_unique<WorldManager>(world);
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
    // 3. initialize systems 
    movementSystem = new MovementSystem(gameContext);
    networkSystem = new NetworkSystem(gameContext);
    networkSyncSystem = new NetworkSyncSystem(gameContext);
    invSystem = new InventorySystem(gameContext);
    behaviorSystem = new BehaviorSystem(gameContext);
    updateSystem = new UpdateSystem(gameContext);
    combatSystem = new CombatSystem(gameContext);
    interactionSystem = new InteractionSystem(gameContext);
    messageSytem = new MessageSystem(gameContext);
    saveSystem = new SaveSystem(gameContext);

    // Add and global entity as 1
    gameContext.registry->CreateEntity();

    gameContext.factories->LoadAllData();

    //4. Initilise scripts that need to be run right away (e.g event listeners)
    //world->LoadWorld("world_data.json", gameContext);
    messageSytem->SubscribeToEvents();
    networkSystem->SetupListeners();
    behaviorSystem->SetupListeners();

}

GameEngine::~GameEngine()
{
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
    interactionSystem->run();
    networkSyncSystem->Run();
    invSystem->Run(deltaTime);
    combatSystem->run();
    updateSystem->Update(deltaTime);
    gameContext.eventBus->CallDefferedCalls();
    saveSystem->Run(deltaTime);
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
		std::stringstream ss = std::stringstream(input.rawText);
        std::string token;
        while (ss >> token) {
			inputStringVector.push_back(token);
        }


		client->stateStack.top()->HandleInput(client, inputStringVector);
    }
}

// Allow the game to close itself (e.g., from a "shutdown" command)
void GameEngine::Quit() { isRunning = false; }

GameContext& GameEngine::GetGameContext()
{
    return gameContext;
}
