#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <optional>
#include "Direction.h"

using EntityID = int;

struct RoomExit {
    Direction direction;
    int targetRoomId;
    int destX = -1;
    int destY = -1;
    bool isPortal = false;
    std::string portalName;
    bool autoTrigger = true;
};

struct RoomExitsComponent {
    std::unordered_map<Direction, RoomExit> exits;
    
    bool HasExit(Direction dir) const {
        return exits.find(dir) != exits.end();
    }
    
    const RoomExit* GetExit(Direction dir) const {
        auto it = exits.find(dir);
        if (it != exits.end()) {
            return &it->second;
        }
        return nullptr;
    }
};

struct RoomIdentityComponent {
    int roomId;
    std::string name;
    std::string description;
    bool isInstance = false;
    int templateId = -1;
};

struct RoomLayoutComponent {
    int width = 0;
    int height = 0;
    std::vector<int> terrainGrid;
    std::unordered_map<char, int> localTerrain;
    
    int GetTerrain(int x, int y) const {
        if (!IsValidCoord(x, y)) return -1;
        return terrainGrid[y * width + x];
    }
    
    bool IsValidCoord(int x, int y) const {
        return x >= 0 && x < width && y >= 0 && y < height;
    }
    
    bool IsWall(int x, int y) const {
        int terrain = GetTerrain(x, y);
        return terrain < 0;
    }
    
    void SetTerrain(int x, int y, int terrainId) {
        if (IsValidCoord(x, y)) {
            terrainGrid[y * width + x] = terrainId;
        }
    }
};

struct RoomSpawnComponent {
    int spawnX = 0;
    int spawnY = 0;
};
