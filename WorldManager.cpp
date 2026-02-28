#include "WorldManager.h"
#include "World.h"
#include "Component.h"
#include "Direction.h"
#include "RoomComponents.h"
#include "Registry.h"
#include <cstdio>

enum MoveType {
    NoMove,
    MicroMove,
    MacroMove
};

WorldManager::WorldManager(World* w, Registry* reg) : world(w), registry(reg)
{
}

WorldManager::~WorldManager()
{
}

EntityID WorldManager::GetRoomEntity(int roomId) {
    // Search for room entity with matching RoomIdentityComponent
    for (EntityID entity : registry->view<RoomIdentityComponent>()) {
        auto* identity = registry->GetComponent<RoomIdentityComponent>(entity);
        if (identity && identity->roomId == roomId) {
            return entity;
        }
    }
    return 0;
}

const RoomLayoutComponent* WorldManager::GetRoomLayout(int roomId) {
    EntityID roomEntity = GetRoomEntity(roomId);
    if (roomEntity == 0) return nullptr;
    return registry->GetComponent<RoomLayoutComponent>(roomEntity);
}

const RoomExitsComponent* WorldManager::GetRoomExits(int roomId) {
    EntityID roomEntity = GetRoomEntity(roomId);
    if (roomEntity == 0) return nullptr;
    return registry->GetComponent<RoomExitsComponent>(roomEntity);
}

const RoomSpawnComponent* WorldManager::GetRoomSpawn(int roomId) {
    EntityID roomEntity = GetRoomEntity(roomId);
    if (roomEntity == 0) return nullptr;
    return registry->GetComponent<RoomSpawnComponent>(roomEntity);
}

int WorldManager::AttemptMove(Direction& dir, PositionComponent* pos, int EntityId) {
    
    const RoomLayoutComponent* currentRoomLayout = GetRoomLayout(pos->roomId);
    if (currentRoomLayout == nullptr) {
        printf("Error: Player is in a void (Room ID %d does not exist).\n", pos->roomId);
        return MoveType::NoMove;
    }

    // --- PHASE 1: MICRO MOVEMENT (Grid Logic) ---
    // Check if room has a grid (width > 0 && height > 0)
    if (currentRoomLayout->width > 0 && currentRoomLayout->height > 0) {

        int targetX = pos->x;
        int targetY = pos->y;

        // Calculate theoretical new position
        switch (dir) {
        case Direction::North: targetY--; break;
        case Direction::South: targetY++; break;
        case Direction::East:  targetX++; break;
        case Direction::West:  targetX--; break;
        }

        if (currentRoomLayout->IsValidCoord(targetX, targetY)) {

            // CHECK B: Is the tile blocked? (Wall/Obstacle)
            if (CanMoveTo(pos->roomId, targetX, targetY)) {
                // Success: Commit the micro-step
                pos->x = targetX;
                pos->y = targetY;
                return MoveType::MicroMove;
            }
            else {
                // Failed: Hit a wall
                return MoveType::MacroMove;
            }
        }
        else {
            // CHECK C: We are OUTSIDE the room (Walked off edge).
            if (HandleMacroMove(EntityId, dir, pos, pos->roomId))
                return MoveType::MacroMove;
        }
    }

    // --- PHASE 2: MACRO MOVEMENT (Abstract Room or Fallthrough) ---
    // If we are here, either the room has no grid, OR we walked off the edge above.
    if (HandleMacroMove(EntityId, dir, pos, pos->roomId))
        return MoveType::MacroMove;
    return MoveType::NoMove;
}

bool WorldManager::HandleMacroMove(int entityId, Direction dir, PositionComponent* pos, int currentRoomId)
{
    const RoomExitsComponent* exitsComponent = GetRoomExits(currentRoomId);
    
    if (exitsComponent == nullptr) {
        return false;
    }

    const RoomExit* exit = exitsComponent->GetExit(dir);
    if (exit == nullptr) {
        return false;
    }

    // Get target room
    const RoomSpawnComponent* targetSpawn = GetRoomSpawn(exit->targetRoomId);
    if (targetSpawn == nullptr && (exit->destX < 0 || exit->destY < 0)) {
        // No target room or no spawn info and no explicit destination
        return false;
    }

    // 3. Update the Entity's Position Component
    pos->roomId = exit->targetRoomId;
    
    // Use explicit destination if provided, otherwise use target room spawn
    if (exit->destX >= 0 && exit->destY >= 0) {
        pos->x = exit->destX;
        pos->y = exit->destY;
    } else if (targetSpawn) {
        pos->x = targetSpawn->spawnX;
        pos->y = targetSpawn->spawnY;
    } else {
        pos->x = 0;
        pos->y = 0;
    }

    return true;
}

bool WorldManager::AttemptTeleport(PositionComponent* pos, int roomId)
{
    if (roomId == -1) return false;
    
    const RoomSpawnComponent* spawn = GetRoomSpawn(roomId);
    if (!spawn) return false;
    
    pos->roomId = roomId;
    pos->x = spawn->spawnX;
    pos->y = spawn->spawnY;

    return true;
}

bool WorldManager::CanMoveTo(int roomId, int x, int y)
{
    const RoomLayoutComponent* layout = GetRoomLayout(roomId);
    if (!layout) return false;
    
    return !layout->IsWall(x, y);
}

bool WorldManager::PutPlayerInRoom(int roomId, PositionComponent& position)
{
    const RoomSpawnComponent* spawn = GetRoomSpawn(roomId);

    if (spawn) {
        position.x = spawn->spawnX;
        position.y = spawn->spawnY;
        position.roomId = roomId;
        return true;
    }

    return false;
}