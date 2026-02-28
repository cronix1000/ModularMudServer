# Room System Migration Summary

## Overview
Successfully migrated from class-based Room system to full ECS (Entity Component System) architecture.

## Changes Made

### 1. New Components Created (RoomComponents.h)
- **RoomIdentityComponent**: roomId, name, description, isInstance, templateId
- **RoomLayoutComponent**: width, height, terrainGrid, localTerrain, helper methods
- **RoomExitsComponent**: exits map with RoomExit struct (supports portals)
- **RoomSpawnComponent**: spawnX, spawnY
- **RoomExit struct**: direction, targetRoomId, destX/Y, isPortal, portalName, autoTrigger

### 2. RoomFactory Created
- **RoomFactory.h/cpp**: Factory pattern for creating room entities
- `CreateRoom(jsonData)`: Creates room from JSON
- `CreateInstancedRoom(templateId)`: Creates copy of template room with ID 10000+
- `GetRoomById(roomId)`: Lookup room entity by ID

### 3. Component.h Updated
- Removed `#include "RoomComponent.h"`
- Added `#include "RoomComponents.h"`

### 4. WorldManager Updated
- Now uses Registry* to query room components
- Added helper methods:
  - `GetRoomLayout(roomId)`
  - `GetRoomExits(roomId)`
  - `GetRoomSpawn(roomId)`
- All movement methods updated to use components instead of Room*
- Constructor now takes `(World*, Registry*)`

### 5. GameEngine.cpp Updated
- WorldManager construction now passes registry:
  ```cpp
  gameContext.worldManager = std::make_unique<WorldManager>(world, gameContext.registry.get());
  ```

### 6. MovementSystem Updated
- Now queries RoomIdentityComponent directly for script execution
- No longer depends on Room::GetEnityID()

### 7. NetworkSystem Updated
- Room event handler now queries RoomIdentityComponent
- Removed #include "Room.h"

### 8. NetworkSyncSystem Updated
- SendLook() now uses RoomLayoutComponent
- Removed #include "Room.h"
- Uses globalTerrain lookup for terrain display

### 9. World Class Updated
- Removed Room* and roomMap
- Added RoomFactory* member
- LoadWorld, LoadRegion, LoadRoomFile now use RoomFactory
- GetRoom() replaced with GetRoomEntity()

### 10. Files Deleted
- Room.h
- Room.cpp
- RoomComponent.h

## Room ID Allocation
- Static rooms: 1 - 9999
- Instanced rooms: 10000+ (incremented automatically)

## Key Features Preserved
- 2D grid-based movement
- Room-to-room exits (north/south/east/west)
- Portal support (ladders, stairs, etc.) via RoomExit.isPortal
- JSON room loading format (unchanged)
- Script hooks (on_enter, on_exit, pulse)
- Instance room creation

## Migration Benefits
1. **Better ECS alignment**: Rooms are now first-class entities
2. **Query flexibility**: Can query rooms using registry views
3. **Memory efficiency**: Components stored in contiguous arrays
4. **Instance support**: Easy creation of room instances
5. **No entityId tracking**: PositionComponent.roomId replaces Room::entityIds

## Backward Compatibility Notes
- JSON room format unchanged
- Exit format supports both simple and portal configurations
- Terrain system still uses char symbols mapped to TerrainDef

## Files Modified
- RoomComponents.h (created)
- RoomFactory.h/cpp (created)
- Component.h
- WorldManager.h/cpp
- GameEngine.cpp
- MovementSystem.cpp
- NetworkSystem.h/cpp
- NetworkSyncSystem.cpp
- World.h/cpp
- NetworkSystem.h (removed Room.h include)

## Files Deleted
- Room.h
- Room.cpp
- RoomComponent.h

## Usage Example

### Creating a Room (from JSON)
```cpp
RoomFactory factory(ctx);
EntityID room = factory.CreateRoom(roomJsonData);
```

### Creating an Instance Room
```cpp
EntityID instance = factory.CreateInstancedRoom(100); // Template room 100
```

### Querying Room Data
```cpp
// In WorldManager or any system:
const RoomLayoutComponent* layout = ctx.worldManager->GetRoomLayout(roomId);
const RoomExitsComponent* exits = ctx.worldManager->GetRoomExits(roomId);
const RoomSpawnComponent* spawn = ctx.worldManager->GetRoomSpawn(roomId);
```

### Getting Entities in Room
```cpp
for (EntityID entity : registry->view<PositionComponent>()) {
    auto* pos = registry->GetComponent<PositionComponent>(entity);
    if (pos && pos->roomId == targetRoomId) {
        // Process entity in room
    }
}
```
