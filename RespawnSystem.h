#pragma once
#include <string>

struct GameContext;

class RespawnSystem {
public:
    GameContext& ctx;
    RespawnSystem(GameContext& g) : ctx(g) {}
    ~RespawnSystem() = default;
    
    // Call every frame with deltaTime
    // Processes DeadTag entities and respawns mobs
    void Update(float deltaTime);
    
    // Create a spawn point that will automatically spawn and respawn mobs
    // Returns the spawn point entity ID
    int CreateSpawnPoint(const std::string& templateId, float respawnTime, int x, int y, int roomId);
};
