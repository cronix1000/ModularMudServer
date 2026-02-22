#include "RespawnSystem.h"
#include "GameContext.h"
#include "Registry.h"
#include "RespawnComponent.h"
#include "MobComponent.h"
#include "PositionComponent.h"
#include "HealthComponent.h"
#include "MobFactory.h"
#include "FactoryManager.h"
#include "Tags.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <vector>

using json = nlohmann::json;

void RespawnSystem::Update(float deltaTime) {
    // First: Process all entities marked as dead and notify their spawn points
    std::vector<int> deadEntities;
    for (EntityID entity : ctx.registry->view<DeadTag>()) {
        deadEntities.push_back(entity);
    }
    
    for (int deadEntity : deadEntities) {
        // Find the spawn point that owns this mob
        for (EntityID spawnEntity : ctx.registry->view<RespawnComponent>()) {
            auto* spawnPoint = ctx.registry->GetComponent<RespawnComponent>(spawnEntity);
            if (!spawnPoint) continue;
            
            if (spawnPoint->currentMobEntityId == deadEntity && spawnPoint->hasLivingMob) {
                // Mark spawn point as ready to respawn
                spawnPoint->hasLivingMob = false;
                spawnPoint->timeRemaining = spawnPoint->respawnTimer;
                std::cout << "Mob " << deadEntity << " died, spawn point " << spawnEntity 
                          << " will respawn in " << spawnPoint->respawnTimer << " seconds" << std::endl;
                break;
            }
        }
        
        // Remove DeadTag and add DestroyTag for CleanUpSystem to handle
        ctx.registry->RemoveComponent<DeadTag>(deadEntity);
        ctx.registry->AddComponent<DestroyTag>(deadEntity, DestroyTag{});
    }
    
    // Second: Find all spawn points that need to respawn (no living mob and timer running)
    std::vector<int> spawnPointsToRespawn;
    
    for (EntityID entity : ctx.registry->view<RespawnComponent>()) {
        auto* spawnPoint = ctx.registry->GetComponent<RespawnComponent>(entity);
        if (!spawnPoint) continue;
        
        // Only process spawn points that don't have a living mob
        if (spawnPoint->hasLivingMob) {
            // Check if the mob is still alive
            auto* health = ctx.registry->GetComponent<HealthComponent>(spawnPoint->currentMobEntityId);
            auto* mob = ctx.registry->GetComponent<MobComponent>(spawnPoint->currentMobEntityId);
            
            // If mob is dead or no longer exists, mark spawn point as ready to respawn
            if (!health || !mob || health->health <= 0) {
                spawnPoint->hasLivingMob = false;
                spawnPoint->timeRemaining = spawnPoint->respawnTimer;
                std::cout << "Mob died at spawn point " << entity << ", respawn in " 
                          << spawnPoint->respawnTimer << " seconds" << std::endl;
            }
            continue;
        }
        
        // Count down the respawn timer
        if (spawnPoint->timeRemaining > 0.0f) {
            spawnPoint->timeRemaining -= deltaTime;
            
            // Time to respawn!
            if (spawnPoint->timeRemaining <= 0.0f) {
                spawnPointsToRespawn.push_back(entity);
            }
        }
    }
    
    // Process respawns
    for (int spawnPointId : spawnPointsToRespawn) {
        auto* spawnPoint = ctx.registry->GetComponent<RespawnComponent>(spawnPointId);
        if (!spawnPoint) continue;
        
        // Spawn the new mob
        int newMobId = ctx.factories->mobs.CreateMob(
            spawnPoint->templateId,
            nlohmann::json::object(),
            spawnPoint->spawnX,
            spawnPoint->spawnY,
            spawnPoint->spawnRoomId
        );
        
        if (newMobId != -1) {
            // Update spawn point to track the new mob
            spawnPoint->currentMobEntityId = newMobId;
            spawnPoint->hasLivingMob = true;
            spawnPoint->timeRemaining = 0.0f;
            
            std::cout << "Mob respawned: " << spawnPoint->templateId << " at (" 
                      << spawnPoint->spawnX << ", " << spawnPoint->spawnY << ") in room " 
                      << spawnPoint->spawnRoomId << " (spawn point " << spawnPointId << ")" << std::endl;
        }
    }
}

int RespawnSystem::CreateSpawnPoint(const std::string& templateId, float respawnTime, 
                                     int x, int y, int roomId) {
    // Create a spawn point entity
    int spawnPointId = ctx.registry->CreateEntity();
    
    RespawnComponent spawnComp;
    spawnComp.templateId = templateId;
    spawnComp.respawnTimer = respawnTime;
    spawnComp.timeRemaining = 0.0f;
    spawnComp.spawnX = x;
    spawnComp.spawnY = y;
    spawnComp.spawnRoomId = roomId;
    spawnComp.currentMobEntityId = -1;
    spawnComp.hasLivingMob = false;
    
    ctx.registry->AddComponent<RespawnComponent>(spawnPointId, spawnComp);
    
    // Immediately spawn the first mob
    int mobId = ctx.factories->mobs.CreateMob(
        templateId,
        nlohmann::json::object(),
        x, y, roomId
    );
    
    if (mobId != -1) {
        auto* spawnPoint = ctx.registry->GetComponent<RespawnComponent>(spawnPointId);
        if (spawnPoint) {
            spawnPoint->currentMobEntityId = mobId;
            spawnPoint->hasLivingMob = true;
        }
    }
    
    return spawnPointId;
}
