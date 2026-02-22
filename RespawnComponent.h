#pragma once
#include <string>

// RespawnComponent is attached to SPAWN POINT entities (not mobs themselves)
// A spawn point tracks where and when to spawn a mob
struct RespawnComponent {
    std::string templateId;    // The mob template ID to respawn (e.g., "goblin_grunt")
    float respawnTimer;        // Seconds until respawn after death
    float timeRemaining;       // Countdown timer (starts at respawnTimer when mob dies)
    int spawnX, spawnY;        // Position where mob should respawn
    int spawnRoomId;           // Room where mob should respawn
    int currentMobEntityId;    // Entity ID of currently living mob (-1 if none/dead)
    bool hasLivingMob;         // Whether there's currently a living mob from this spawn point
    
    RespawnComponent() : respawnTimer(30.0f), timeRemaining(0.0f), 
                         spawnX(0), spawnY(0), spawnRoomId(-1), 
                         currentMobEntityId(-1), hasLivingMob(false) {}
};
