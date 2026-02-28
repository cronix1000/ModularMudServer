#pragma once
#include "GameContext.h"
#include "Entity.h"
#include "AmbientAIComponent.h"
#include "ZoneClimateComponent.h"
#include <unordered_map>
#include <vector>
#include <string>

using EntityID = int;

/**
 * AmbientAISystem - Controls non-combat NPC behaviors
 * 
 * PERFORMANCE OPTIMIZATIONS:
 * - Only updates mobs in zones with players
 * - Staggered update intervals (not all mobs update same frame)
 * - Configurable update rate per behavior type
 * - Early exit for inactive zones
 */
class AmbientAISystem {
public:
    explicit AmbientAISystem(GameContext& ctx);
    ~AmbientAISystem() = default;
    
    // Main update - call from GameEngine
    void Update(float deltaTime);
    
    // Individual behavior updates
    void UpdateWander(EntityID entity, AmbientAIComponent& ai, float deltaTime);
    void UpdatePatrol(EntityID entity, AmbientAIComponent& ai, float deltaTime);
    void UpdateGuard(EntityID entity, AmbientAIComponent& ai, float deltaTime);
    void UpdateMerchant(EntityID entity, AmbientAIComponent& ai, float deltaTime);
    void UpdateScheduled(EntityID entity, AmbientAIComponent& ai, float deltaTime);
    
    // Time-sensitive mob updates (sleep/wake cycles)
    void UpdateTimeSensitive(EntityID entity, TimeSensitiveComponent& ts, 
                              const ZoneClimateComponent& climate);
    
    // Initialization helpers
    void SetupWander(EntityID entity, int homeRoomId, int homeX, int homeY, 
                     uint8_t radius, float cooldown);
    void SetupPatrol(EntityID entity, const std::vector<AmbientAIComponent::Waypoint>& waypoints,
                     bool loop = true);
    void SetupGuard(EntityID entity, uint8_t detectionRadius, const std::string& targetFaction);
    void SetupMerchant(EntityID entity, float shoutCooldown, 
                       const std::vector<std::string>& messages);
    void SetupTimeSensitive(EntityID entity, TimeSensitiveComponent::ActivityPattern pattern,
                            uint8_t wakeHour = 6, uint8_t sleepHour = 22);
    
    // Event handlers
    void OnHourChanged(const std::string& zoneId, uint8_t hour);
    void OnPlayerEnteredZone(const std::string& zoneId);
    void OnPlayerExitedZone(const std::string& zoneId);
    
    // Force update specific entity
    void ForceUpdate(EntityID entity);
    
    // Configuration
    void SetUpdateInterval(float seconds) { updateInterval = seconds; }
    
private:
    GameContext& ctx;
    
    // Performance settings
    float updateInterval = 1.0f;     // Update AI every 1 second (not every tick)
    float timeAccumulator = 0.0f;
    
    // Staggering: different behavior types update at different rates
    static constexpr float WANDER_UPDATE_RATE = 1.0f;    // Every second
    static constexpr float PATROL_UPDATE_RATE = 0.5f;    // Twice per second
    static constexpr float GUARD_UPDATE_RATE = 0.2f;     // 5 times per second (responsive)
    static constexpr float MERCHANT_UPDATE_RATE = 5.0f;  // Every 5 seconds
    static constexpr float SCHEDULED_UPDATE_RATE = 1.0f; // Every second
    
    // Track which zones have players (optimization)
    std::unordered_map<std::string, bool> activeZones;
    
    // Helper functions
    bool ShouldUpdateThisFrame(const AmbientAIComponent& ai, float deltaTime);
    bool CanMoveTo(int roomId, int x, int y);
    void MoveEntity(EntityID entity, int targetRoomId, int targetX, int targetY);
    void SendEmote(EntityID entity, const std::string& message);
    void BroadcastToRoom(int roomId, const std::string& message, EntityID exclude = -1);
    
    // Patrol helpers
    void AdvanceWaypoint(AmbientAIComponent& ai);
    AmbientAIComponent::Waypoint* GetCurrentWaypoint(AmbientAIComponent& ai);
    
    // Guard helpers
    EntityID FindTargetInRange(EntityID guard, uint8_t radius, const std::string& faction);
    void TriggerConfrontation(EntityID guard, EntityID target);
    
    // Random helpers
    float RandomFloat();
    int RandomInt(int min, int max);
    bool RollChance(float chance);  // 0.0-1.0
};
