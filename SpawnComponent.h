#pragma once
#include <string>
struct SpawnData {
    std::string mobID; // "goblin_grunt"
    int x, y;
    float respawnTimer; // 300 seconds
    float timeOfDeath;  // -1 if alive
    int currentEntityID; // ID of the live mob (or -1 if dead)
};