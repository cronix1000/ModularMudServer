#pragma once

using EntityID = int;

// Forward declarations
class World;
class Registry;
class RoomFactory;
enum class Direction;
struct PositionComponent;
struct RoomLayoutComponent;
struct RoomExitsComponent;
struct RoomSpawnComponent;

class WorldManager
{
public:
	WorldManager(World* w, Registry* reg);
	~WorldManager();
	
	World* world;
	Registry* registry;
	
	int AttemptMove(Direction& dir, PositionComponent* pos, int EntityId);
	bool HandleMacroMove(int entityId, Direction dir, PositionComponent* pos, int currentRoomId);
	bool CanMoveTo(int roomId, int x, int y);
	bool AttemptTeleport(PositionComponent* pos, int roomId);
	bool PutPlayerInRoom(int roomId, PositionComponent& position);
	
	// Helper methods to get room components
	const RoomLayoutComponent* GetRoomLayout(int roomId);
	const RoomExitsComponent* GetRoomExits(int roomId);
	const RoomSpawnComponent* GetRoomSpawn(int roomId);
	
private:
	EntityID GetRoomEntity(int roomId);
};