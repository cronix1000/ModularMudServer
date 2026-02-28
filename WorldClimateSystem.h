#pragma once
#include "GameContext.h"
#include "ZoneClimateComponent.h"
#include <string>
#include <unordered_map>
#include <vector>

/**
 * WorldClimateSystem - Manages weather and time per zone/floor
 * 
 * PERFORMANCE OPTIMIZATIONS:
 * - Updates at configurable interval (default 1 sec), not every tick
 * - Only processes zones with active players
 * - Caches zone lookups by zoneId string
 * - Throttles updates for empty zones
 * - Batches effect calculations
 */
class WorldClimateSystem {
public:
    explicit WorldClimateSystem(GameContext& ctx);
    ~WorldClimateSystem() = default;
    
    // Main update - call from GameEngine::Update
    void Update(float deltaTime);
    
    // Zone management
    int CreateClimateZone(const std::string& zoneId, const std::string& zoneName, 
                          ZoneClimateComponent::ClimateType type = ZoneClimateComponent::ClimateType::TEMPERATE,
                          bool isOutdoor = true);
    
    // Get climate for a zone
    ZoneClimateComponent* GetZoneClimate(const std::string& zoneId);
    ZoneClimateComponent* GetZoneClimate(int entityId);
    
    // Player activity tracking (for optimization)
    void OnPlayerEnteredZone(const std::string& zoneId);
    void OnPlayerExitedZone(const std::string& zoneId);
    
    // Time/weather queries
    std::string GetTimeString(const std::string& zoneId);
    std::string GetWeatherString(const std::string& zoneId);
    std::string GetAtmosphericDescription(const std::string& zoneId);
    
    // Manual weather control (for admin/events)
    void SetWeather(const std::string& zoneId, WeatherCondition weather);
    void SetTime(const std::string& zoneId, uint8_t hour, uint8_t day, uint8_t month);
    void ForceWeatherChange(const std::string& zoneId);
    
    // Apply environmental effects to entity
    void ApplyEnvironmentalEffects(int entityId, const std::string& zoneId);
    
    // Configuration
    void SetUpdateInterval(float seconds) { updateInterval = seconds; }
    void SetTimeScale(float scale) { timeScale = scale; }
    
    // Get all active zones (for Lua API)
    std::vector<std::string> GetActiveZones() const;
    
private:
    GameContext& ctx;
    
    // Performance settings
    float updateInterval = 1.0f;     // Update climate every 1 second (not every tick)
    float timeAccumulator = 0.0f;    // Time since last update
    float timeScale = 60.0f;         // Game time multiplier (1 real sec = 60 game sec = 1 game min)
    
    // Zone cache for fast lookups
    std::unordered_map<std::string, int> zoneEntityCache;  // zoneId -> entityId
    
    // Throttling for empty zones (seconds between updates when no players)
    static constexpr float EMPTY_ZONE_UPDATE_INTERVAL = 10.0f;
    
    // Climate update functions
    void UpdateTime(ZoneClimateComponent& climate, float deltaTime);
    void UpdateWeather(ZoneClimateComponent& climate);
    void CalculateEnvironmentalEffects(ZoneClimateComponent& climate);
    
    // Weather transition logic
    WeatherCondition DetermineNextWeather(const ZoneClimateComponent& climate);
    float CalculateWeatherChangeChance(ZoneClimateComponent::ClimateType type, Season season);
    
    // Effect application
    void ApplyEffectsToEntitiesInZone(const std::string& zoneId, const ZoneClimateComponent& climate);
    void ClearEnvironmentalEffects(int entityId);
    
    // Helper functions
    int8_t CalculateTemperature(const ZoneClimateComponent& climate);
    uint8_t CalculateVisibility(const ZoneClimateComponent& climate);
    std::string GenerateWeatherTransitionMessage(WeatherCondition from, WeatherCondition to);
    std::string GenerateTimePhaseMessage(TimePhase phase);
    
    // Event broadcasting
    void BroadcastWeatherChange(const std::string& zoneId, WeatherCondition oldWeather, WeatherCondition newWeather);
    void BroadcastTimePhaseChange(const std::string& zoneId, TimePhase oldPhase, TimePhase newPhase);
    
    // Season progression
    void AdvanceSeason(ZoneClimateComponent& climate);
    
    // Random number generation (fast, non-crypto)
    float RandomFloat();
    int RandomInt(int min, int max);
};
