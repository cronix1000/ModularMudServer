#pragma once
#include "World.h"
#include "Component.h"

class WorldManager
{
public:
	WorldManager(World* w);
	~WorldManager();
	World* world;
	int AttemptMove(Direction& dir, PositionComponent* pos, int EntityId);
	bool HandleMacroMove(int entityId, Direction dir, PositionComponent* pos, Room* currentRoom);
	bool CanMoveTo(Room* room, int x, int y);
	bool AttemptTeleport(PositionComponent* pos, int roomId);
	void PutPlayerInRoom(int roomId, PositionComponent& position);
private:

};
