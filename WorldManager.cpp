#include "WorldManager.h"
#include "World.h"
#include "Room.h"        
#include "Direction.h"   
#include "Component.h"
#include <cstdio>      

enum MoveType {
    NoMove,
    MicroMove,
    MacroMove
};

WorldManager::WorldManager(World* w) : world(w)
{
}

WorldManager::~WorldManager()
{
}

int WorldManager::AttemptMove(Direction& dir, PositionComponent* pos, int EntityId) {
    
    Room* currentRoom = world->GetRoom(pos->roomId);
    if (currentRoom == nullptr) {
        printf("Error: Player is in a void (Room ID %d does not exist).\n", pos->roomId);
        return MoveType::NoMove;
    }

    // --- PHASE 1: MICRO MOVEMENT (Grid Logic) ---
    if (currentRoom->HasGrid()) {

        int targetX = pos->x;
        int targetY = pos->y;

        // Calculate theoretical new position
        switch (dir) {
        case Direction::North: targetY--; break;
        case Direction::South: targetY++; break;
        case Direction::East:  targetX++; break;
        case Direction::West:  targetX--; break;
        }

        if (currentRoom->IsValidCoord(targetX, targetY)) {

            // CHECK B: Is the tile blocked? (Wall/Obstacle)
            if (CanMoveTo(currentRoom, targetX, targetY)) {
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
            if (HandleMacroMove(EntityId, dir, pos, currentRoom))
                return MoveType::MacroMove;
        }
    }

    // --- PHASE 2: MACRO MOVEMENT (Abstract Room or Fallthrough) ---
    // If we are here, either the room has no grid, OR we walked off the edge above.
    if (HandleMacroMove(EntityId, dir, pos, currentRoom))
        return MoveType::MacroMove;
    return MoveType::NoMove;
    }

bool WorldManager::HandleMacroMove(int entityId, Direction dir, PositionComponent* pos, Room* currentRoom)
{
    // 1. Remove from Old Room (Linear search on a SMALL list is fast)
    Room* oldRoom = currentRoom;

    if (oldRoom == nullptr) {
        return false;
    }

    ExitData exit = oldRoom->GetExit(dir);

    // 2. Add to New Room
    Room* newRoom = world->GetRoom(exit.targetRoomID);
    if (newRoom == nullptr) {
        return false;
    }

    // 3. Update the Entity's Position Component
    pos->roomId = exit.targetRoomID;
    pos->x = exit.destX;
    pos->y = exit.destY;

    return true;
}

bool WorldManager::AttemptTeleport(PositionComponent* pos, int roomId)
{
    if (roomId == -1) return false;
    Room* newRoom = world->GetRoom(roomId);
    
    if (!newRoom) return false;
    pos->roomId = roomId;
    pos->x = newRoom->spawn.first;
    pos->y = newRoom->spawn.second;

    return true;
}

bool WorldManager::CanMoveTo(Room* room, int x, int y)
{
    if (room->IsWall(x, y)) return false;
        return true;
}

bool WorldManager::PutPlayerInRoom(int roomId, PositionComponent& position)
{
    Room* room = world->GetRoom(roomId);

    if (room) {
        position.x = room->spawn.first;
        position.y = room->spawn.second;
        position.roomId = roomId;
        return true;
    }

    return false;
}
