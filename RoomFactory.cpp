#include "RoomFactory.h"
#include "RoomComponents.h"
#include "ScriptComponent.h"
#include "GameContext.h"
#include "Direction.h"
#include "TerrainDef.h"
#include <iostream>
#include "Registry.h"
#include "TextHelperFunctions.h"

using EntityID = int;

//// Helper to convert string direction to Direction enum
//Direction StringToDirection(const std::string& str) {
//    if (str == "north") return Direction::North;
//    if (str == "south") return Direction::South;
//    if (str == "east") return Direction::East;
//    if (str == "west") return Direction::West;
//    if (str == "up") return Direction::Up;
//    if (str == "down") return Direction::Down;
//    return Direction::North; // Default
//}

RoomFactory::RoomFactory(GameContext& context) : ctx(context), nextInstanceId(10000) {
}

EntityID RoomFactory::CreateRoom(const json& roomData) {
    return CreateRoomInternal(roomData, false, -1);
}

EntityID RoomFactory::CreateInstancedRoom(int templateRoomId) {
    // Find the template room
    EntityID templateEntity = GetRoomById(templateRoomId);
    if (templateEntity == 0) {
        std::cerr << "RoomFactory: Template room " << templateRoomId << " not found" << std::endl;
        return 0;
    }
    
    // Get template components
    auto* identity = ctx.registry->GetComponent<RoomIdentityComponent>(templateEntity);
    auto* layout = ctx.registry->GetComponent<RoomLayoutComponent>(templateEntity);
    auto* exits = ctx.registry->GetComponent<RoomExitsComponent>(templateEntity);
    auto* spawn = ctx.registry->GetComponent<RoomSpawnComponent>(templateEntity);
    auto* scripts = ctx.registry->GetComponent<ScriptComponent>(templateEntity);
    
    if (!identity || !layout) {
        std::cerr << "RoomFactory: Template room missing required components" << std::endl;
        return 0;
    }
    
    // Create new entity
    EntityID instanceEntity = ctx.registry->CreateEntity();
    int instanceId = nextInstanceId++;
    
    // Copy identity with instance flag
    ctx.registry->AddComponent<RoomIdentityComponent>(instanceEntity, {
        instanceId,
        identity->name + " (Instance)",
        identity->description,
        true,  // isInstance
        templateRoomId
    });
    
    // Copy layout
    ctx.registry->AddComponent<RoomLayoutComponent>(instanceEntity, {
        layout->width,
        layout->height,
        layout->terrainGrid,  // Copy grid
        layout->localTerrain  // Copy terrain legend
    });
    
    // Copy exits if present
    if (exits) {
        ctx.registry->AddComponent<RoomExitsComponent>(instanceEntity, {
            exits->exits
        });
    }
    
    // Copy spawn if present
    if (spawn) {
        ctx.registry->AddComponent<RoomSpawnComponent>(instanceEntity, {
            spawn->spawnX,
            spawn->spawnY
        });
    }
    
    // Copy scripts if present
    if (scripts) {
        ctx.registry->AddComponent<ScriptComponent>(instanceEntity, *scripts);
    }
    
    // Register in lookup
    roomIdToEntity[instanceId] = instanceEntity;
    
    return instanceEntity;
}

EntityID RoomFactory::GetRoomById(int roomId) {
    // Check cache first
    auto it = roomIdToEntity.find(roomId);
    if (it != roomIdToEntity.end()) {
        return it->second;
    }
    
    // Search registry for room with matching RoomIdentityComponent
    for (EntityID entity : ctx.registry->view<RoomIdentityComponent>()) {
        auto* identity = ctx.registry->GetComponent<RoomIdentityComponent>(entity);
        if (identity && identity->roomId == roomId) {
            roomIdToEntity[roomId] = entity;  // Cache it
            return entity;
        }
    }
    
    return 0;  // Not found
}

EntityID RoomFactory::CreateRoomInternal(const json& roomData, bool isInstance, int templateId) {
    // Create entity
    EntityID roomEntity = ctx.registry->CreateEntity();
    int roomId = roomData.value("id", roomEntity);  // Use provided ID or entity ID
    
    // 1. Add RoomIdentityComponent
    RoomIdentityComponent identity;
    identity.roomId = roomId;
    identity.name = roomData.value("name", "Unnamed Room");
    identity.description = roomData.value("description", "");
    identity.isInstance = isInstance;
    identity.templateId = templateId;
    
    ctx.registry->AddComponent<RoomIdentityComponent>(roomEntity, identity);
    
    // 2. Add RoomLayoutComponent (if grid-based)
    int width = roomData.value("width", 0);
    int height = roomData.value("height", 0);
    
    if (width > 0 && height > 0) {
        RoomLayoutComponent layout;
        layout.width = width;
        layout.height = height;
        layout.terrainGrid.resize(width * height, -1);  // -1 = void/empty
        
        // Parse layout if present
        if (roomData.contains("layout")) {
            ParseLayout(layout, roomData["layout"], roomData);
        }
        
        ctx.registry->AddComponent<RoomLayoutComponent>(roomEntity, layout);
    }
    
    // 3. Add RoomExitsComponent
    if (roomData.contains("exits")) {
        RoomExitsComponent exits;
        ParseExits(exits, roomData["exits"]);
        ctx.registry->AddComponent<RoomExitsComponent>(roomEntity, exits);
    }
    
    // 4. Add RoomSpawnComponent
    if (roomData.contains("spawn")) {
        RoomSpawnComponent spawn;
        spawn.spawnX = roomData["spawn"].value("x", width / 2);
        spawn.spawnY = roomData["spawn"].value("y", height / 2);
        ctx.registry->AddComponent<RoomSpawnComponent>(roomEntity, spawn);
    } else if (width > 0 && height > 0) {
        // Default spawn to center
        RoomSpawnComponent spawn;
        spawn.spawnX = width / 2;
        spawn.spawnY = height / 2;
        ctx.registry->AddComponent<RoomSpawnComponent>(roomEntity, spawn);
    }
    
    // 5. Add ScriptComponent if scripts present
    if (roomData.contains("scripts")) {
        ScriptComponent scripts;
        auto& sData = roomData["scripts"];
        if (sData.contains("on_enter")) {
            scripts.scripts_path["on_enter"] = sData["on_enter"];
        }
        if (sData.contains("on_exit")) {
            scripts.scripts_path["on_exit"] = sData["on_exit"];
        }
        if (sData.contains("on_pulse") || sData.contains("pulse")) {
            scripts.scripts_path["pulse"] = sData.value("on_pulse", sData.value("pulse", ""));
        }
        ctx.registry->AddComponent<ScriptComponent>(roomEntity, scripts);
    }
    
    // Register in lookup
    roomIdToEntity[roomId] = roomEntity;
    
    return roomEntity;
}

void RoomFactory::ParseLayout(RoomLayoutComponent& layout, const json& layoutData, const json& roomData) {
    if (!layoutData.is_array()) return;
    
    // Load local terrain legend if present
    std::unordered_map<char, int> localTerrainIds;
    if (roomData.contains("floor_legend")) {
        int nextId = 1;  // Start local IDs from 1
        for (auto& [key, val] : roomData["floor_legend"].items()) {
            if (!key.empty()) {
                localTerrainIds[key[0]] = nextId++;
                // Store the mapping in component for later lookup
                layout.localTerrain[key[0]] = localTerrainIds[key[0]];
            }
        }
    }
    
    int y = 0;
    for (const auto& line : layoutData) {
        if (!line.is_string()) continue;
        std::string row = line;
        int x = 0;
        
        for (char c : row) {
            if (c == ' ') continue;  // Skip spaces
            
            if (x >= layout.width) break;
            
            // Determine terrain ID
            int terrainId = -1;  // Default void
            
            // Check local terrain first
            auto localIt = localTerrainIds.find(c);
            if (localIt != localTerrainIds.end()) {
                terrainId = localIt->second;
            } else if (globalTerrain.count(c)) {
                // Use char value as ID for global terrain (simple approach)
                terrainId = static_cast<int>(c);
            } else if (globalTerrain.count('.')) {
                // Default to floor
                terrainId = static_cast<int>('.');
            }
            
            layout.SetTerrain(x, y, terrainId);
            x++;
        }
        
        y++;
        if (y >= layout.height) break;
    }
}

void RoomFactory::ParseExits(RoomExitsComponent& exits, const json& exitsData) {
    for (auto& [dirString, exitValue] : exitsData.items()) {
        Direction dir = TextHelperFunctions::StringToDirection(dirString);
        RoomExit exit;
        exit.direction = dir;
        
        // Handle Object format: "north": { "target_room": 2, ... }
        if (exitValue.is_object()) {
            exit.targetRoomId = exitValue.value("target_room", -1);
            exit.destX = exitValue.value("dest_x", -1);
            exit.destY = exitValue.value("dest_y", -1);
            exit.isPortal = exitValue.value("is_portal", false);
            exit.portalName = exitValue.value("portal_name", "");
            exit.autoTrigger = exitValue.value("auto_trigger", true);
        }
        // Handle simple format: "north": 2
        else if (exitValue.is_number_integer()) {
            exit.targetRoomId = static_cast<int>(exitValue);
            exit.destX = -1;
            exit.destY = -1;
            exit.isPortal = false;
            exit.autoTrigger = true;
        }
        
        if (exit.targetRoomId >= 0) {
            exits.exits[dir] = exit;
        }
    }
}
