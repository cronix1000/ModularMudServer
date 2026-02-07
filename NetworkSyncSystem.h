#pragma once
#include "Registry.h"
#include "WorldManager.h"
#include "TerrainDef.h"
#include "EventBus.h"
#include "GameContext.h";

class NetworkSyncSystem {
	GameContext& ctx;
public:
	NetworkSyncSystem(GameContext& c);
	
	~NetworkSyncSystem() = default;
	void Run();
	void SendMapUpdate(ClientConnection* clientConnection);
	void SendLook(ClientConnection* client);
private:
};