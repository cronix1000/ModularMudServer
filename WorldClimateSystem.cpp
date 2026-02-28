#include "WorldClimateSystem.h"
#include "Registry.h"
#include "RegionComponent.h"
#include "PositionComponent.h"
#include "ClientComponent.h"
#include "ScriptManager.h"
#include "MobComponent.h"
#include <cmath>
#include <chrono>
#include <algorithm>

WorldClimateSystem::WorldClimateSystem(GameContext& ctx) : ctx(ctx) {
    // Seed random number generator with current time
    srand(static_cast<unsigned>(time(nullptr)));
}

void WorldClimateSystem::Update(float deltaTime) {
    timeAccumulator += deltaTime;
    
    // Only update climate at configured interval (default 1 second)
    // This is a MAJOR performance optimization - not running every tick
    if (timeAccumulator < updateInterval) {
        return;
    }
    
    timeAccumulator -= updateInterval;
    
    // Process all climate zones
    for (EntityID entity : ctx.registry->view<ZoneClimateComponent>()) {
        auto* climate = ctx.registry->GetComponent<ZoneClimateComponent>(entity);
        if (!climate) continue;
        
        // PERFORMANCE: Throttle empty zones
        // If no players in zone, only update every 10 seconds instead of every second
        if (!climate->hasActivePlayers) {
            climate->timeSinceLastUpdate += updateInterval;
            if (climate->timeSinceLastUpdate < EMPTY_ZONE_UPDATE_INTERVAL) {
                continue;  // Skip this update
            }
            climate->timeSinceLastUpdate = 0.0f;
        }
        
        // Update game time
        UpdateTime(*climate, updateInterval);
        
        // Update weather (only for outdoor zones)
        if (climate->isOutdoor) {
            UpdateWeather(*climate);
        }
        
        // Apply environmental effects to entities in this zone
        if (climate->hasActivePlayers) {
            ApplyEffectsToEntitiesInZone(climate->zoneId, *climate);
        }
    }
}

void WorldClimateSystem::UpdateTime(ZoneClimateComponent& climate, float deltaTime) {
    // Convert real time to game time
    // timeScale: 1 real second = X game seconds
    // Default: 60 = 1 real second = 1 game minute
    float gameSecondsElapsed = deltaTime * timeScale;
    climate.timeAccumulator += gameSecondsElapsed;
    
    // Check if we've accumulated a full game minute
    while (climate.timeAccumulator >= 60.0f) {
        climate.timeAccumulator -= 60.0f;
        
        // Store old phase for change detection
        TimePhase oldPhase = climate.currentPhase;
        
        // Increment game hour
        climate.gameHour++;
        
        // Check for day rollover
        if (climate.gameHour >= 24) {
            climate.gameHour = 0;
            climate.gameDay++;
            
            // Check for season/month rollover
            if (climate.gameDay > 30) {
                climate.gameDay = 1;
                climate.gameMonth++;
                
                if (climate.gameMonth >= 4) {
                    climate.gameMonth = 0;
                    climate.gameYear++;
                }
                
                AdvanceSeason(climate);
            }
        }
        
        // Update cached time phase
        climate.currentPhase = ClimateUtils::HourToPhase(climate.gameHour);
        
        // Broadcast phase change if different
        if (climate.currentPhase != oldPhase) {
            BroadcastTimePhaseChange(climate.zoneId, oldPhase, climate.currentPhase);
        }
        
        // Fire Lua event for hour change
        ctx.scripts->execute_hook("on_hour_changed", climate.zoneId, climate.gameHour);
    }
}

void WorldClimateSystem::UpdateWeather(ZoneClimateComponent& climate) {
    // Calculate weather change chance based on climate type and season
    float changeChance = CalculateWeatherChangeChance(climate.climateType, 
                                                       static_cast<Season>(climate.gameMonth));
    
    // Roll for weather change
    if (RandomFloat() > changeChance) {
        return;  // No change this hour
    }
    
    // Determine next weather state
    WeatherCondition newWeather = DetermineNextWeather(climate);
    
    if (newWeather != climate.currentWeather) {
        WeatherCondition oldWeather = climate.currentWeather;
        
        // Start transition
        climate.targetWeather = newWeather;
        climate.weatherTransition = 0.0f;
        
        // For now, instant change (can be smoothed later)
        climate.currentWeather = newWeather;
        climate.targetWeather = newWeather;
        
        // Update derived properties
        climate.temperature = CalculateTemperature(climate);
        climate.visibility = CalculateVisibility(climate);
        
        // Broadcast change
        BroadcastWeatherChange(climate.zoneId, oldWeather, newWeather);
        
        // Fire Lua event
        ctx.scripts->execute_hook("on_weather_changed", climate.zoneId, 
                                   static_cast<int>(oldWeather), 
                                   static_cast<int>(newWeather));
    }
}

WeatherCondition WorldClimateSystem::DetermineNextWeather(const ZoneClimateComponent& climate) {
    // Simple Markov chain for weather transitions
    // Each weather state has probabilities for what it can transition to
    
    WeatherCondition current = climate.currentWeather;
    float roll = RandomFloat();
    
    // Define transition probabilities
    // Format: {weather1, prob1, weather2, prob2, ...}
    // Probabilities should sum to 1.0
    switch (current) {
        case WeatherCondition::CLEAR:
            if (roll < 0.70f) return WeatherCondition::CLEAR;
            if (roll < 0.90f) return WeatherCondition::PARTLY_CLOUDY;
            return WeatherCondition::CLOUDY;
            
        case WeatherCondition::PARTLY_CLOUDY:
            if (roll < 0.40f) return WeatherCondition::CLEAR;
            if (roll < 0.70f) return WeatherCondition::PARTLY_CLOUDY;
            if (roll < 0.90f) return WeatherCondition::CLOUDY;
            return WeatherCondition::OVERCAST;
            
        case WeatherCondition::CLOUDY:
            if (roll < 0.30f) return WeatherCondition::PARTLY_CLOUDY;
            if (roll < 0.60f) return WeatherCondition::CLOUDY;
            if (roll < 0.80f) return WeatherCondition::OVERCAST;
            return WeatherCondition::LIGHT_RAIN;
            
        case WeatherCondition::OVERCAST:
            if (roll < 0.30f) return WeatherCondition::CLOUDY;
            if (roll < 0.50f) return WeatherCondition::OVERCAST;
            if (roll < 0.80f) return WeatherCondition::LIGHT_RAIN;
            return WeatherCondition::HEAVY_RAIN;
            
        case WeatherCondition::LIGHT_RAIN:
            if (roll < 0.20f) return WeatherCondition::CLOUDY;
            if (roll < 0.40f) return WeatherCondition::OVERCAST;
            if (roll < 0.70f) return WeatherCondition::LIGHT_RAIN;
            if (roll < 0.90f) return WeatherCondition::HEAVY_RAIN;
            return WeatherCondition::STORM;
            
        case WeatherCondition::HEAVY_RAIN:
            if (roll < 0.20f) return WeatherCondition::LIGHT_RAIN;
            if (roll < 0.50f) return WeatherCondition::HEAVY_RAIN;
            if (roll < 0.80f) return WeatherCondition::STORM;
            return WeatherCondition::LIGHT_RAIN;  // Storms break
            
        case WeatherCondition::STORM:
            if (roll < 0.40f) return WeatherCondition::STORM;  // Storms persist
            if (roll < 0.70f) return WeatherCondition::HEAVY_RAIN;
            return WeatherCondition::LIGHT_RAIN;
            
        case WeatherCondition::FOG:
            if (roll < 0.60f) return WeatherCondition::CLEAR;
            return WeatherCondition::FOG;
            
        case WeatherCondition::SNOW:
            if (roll < 0.30f) return WeatherCondition::CLEAR;
            if (roll < 0.70f) return WeatherCondition::SNOW;
            return WeatherCondition::LIGHT_RAIN;  // Snow melts
            
        default:
            return WeatherCondition::CLEAR;
    }
}

float WorldClimateSystem::CalculateWeatherChangeChance(ZoneClimateComponent::ClimateType type, Season season) {
    float baseChance = 0.15f;  // 15% base chance per check
    
    // Climate type modifiers
    switch (type) {
        case ZoneClimateComponent::ClimateType::TEMPERATE:
            break;  // Base chance
        case ZoneClimateComponent::ClimateType::TROPICAL:
            baseChance += 0.10f;  // More changeable
            break;
        case ZoneClimateComponent::ClimateType::ARID:
            baseChance -= 0.10f;  // More stable
            break;
        case ZoneClimateComponent::ClimateType::ARCTIC:
            baseChance -= 0.05f;
            break;
        default:
            break;
    }
    
    // Season modifiers
    switch (season) {
        case Season::SPRING:
            baseChance += 0.10f;  // Unpredictable spring weather
            break;
        case Season::SUMMER:
            baseChance -= 0.05f;  // More stable
            break;
        case Season::AUTUMN:
            baseChance += 0.15f;  // Very changeable
            break;
        case Season::WINTER:
            baseChance -= 0.05f;
            break;
    }
    
    return (std::max)(0.05f, (std::min)(0.50f, baseChance));  // Clamp 5-50%
}

int8_t WorldClimateSystem::CalculateTemperature(const ZoneClimateComponent& climate) {
    // Base temperature from season and time
    int8_t base = ClimateUtils::CalculateBaseTemperature(
        static_cast<Season>(climate.gameMonth), 
        climate.currentPhase
    );
    
    // Weather modifiers
    switch (climate.currentWeather) {
        case WeatherCondition::CLEAR:
            base += 5;
            break;
        case WeatherCondition::PARTLY_CLOUDY:
            // No change
            break;
        case WeatherCondition::CLOUDY:
            base -= 3;
            break;
        case WeatherCondition::OVERCAST:
            base -= 5;
            break;
        case WeatherCondition::LIGHT_RAIN:
            base -= 8;
            break;
        case WeatherCondition::HEAVY_RAIN:
            base -= 12;
            break;
        case WeatherCondition::STORM:
            base -= 15;
            break;
        case WeatherCondition::FOG:
            base -= 5;
            break;
        case WeatherCondition::SNOW:
            base -= 20;
            break;
        default:
            break;
    }
    
    // Add some randomness (-2 to +2 degrees)
    base += RandomInt(-2, 2);
    
    return base;
}

uint8_t WorldClimateSystem::CalculateVisibility(const ZoneClimateComponent& climate) {
    switch (climate.currentWeather) {
        case WeatherCondition::CLEAR:
        case WeatherCondition::PARTLY_CLOUDY:
            return 255;  // Unlimited
        case WeatherCondition::CLOUDY:
        case WeatherCondition::OVERCAST:
            return 200;  // ~200 feet
        case WeatherCondition::LIGHT_RAIN:
            return 150;
        case WeatherCondition::HEAVY_RAIN:
            return 80;
        case WeatherCondition::STORM:
            return 40;
        case WeatherCondition::FOG:
            return 30;
        case WeatherCondition::SNOW:
            return 60;
        default:
            return 255;
    }
}

int WorldClimateSystem::CreateClimateZone(const std::string& zoneId, const std::string& zoneName,
                                           ZoneClimateComponent::ClimateType type, bool isOutdoor) {
    EntityID entity = ctx.registry->CreateEntity();
    
    ZoneClimateComponent climate;
    climate.zoneId = zoneId;
    climate.zoneName = zoneName;
    climate.climateType = type;
    climate.isOutdoor = isOutdoor;
    
    // Random initial weather based on climate
    climate.currentWeather = WeatherCondition::CLEAR;
    climate.temperature = CalculateTemperature(climate);
    climate.visibility = CalculateVisibility(climate);
    
    ctx.registry->AddComponent<ZoneClimateComponent>(entity, climate);
    
    // Cache the mapping
    zoneEntityCache[zoneId] = entity;
    
    return entity;
}

ZoneClimateComponent* WorldClimateSystem::GetZoneClimate(const std::string& zoneId) {
    auto it = zoneEntityCache.find(zoneId);
    if (it != zoneEntityCache.end()) {
        return ctx.registry->GetComponent<ZoneClimateComponent>(it->second);
    }
    
    // Fallback: search all zones (for zones created before cache)
    for (EntityID entity : ctx.registry->view<ZoneClimateComponent>()) {
        auto* climate = ctx.registry->GetComponent<ZoneClimateComponent>(entity);
        if (climate && climate->zoneId == zoneId) {
            zoneEntityCache[zoneId] = entity;  // Cache for next time
            return climate;
        }
    }
    
    return nullptr;
}

ZoneClimateComponent* WorldClimateSystem::GetZoneClimate(int entityId) {
    return ctx.registry->GetComponent<ZoneClimateComponent>(entityId);
}

void WorldClimateSystem::OnPlayerEnteredZone(const std::string& zoneId) {
    auto* climate = GetZoneClimate(zoneId);
    if (climate) {
        climate->playerCount++;
        climate->hasActivePlayers = true;
        climate->timeSinceLastUpdate = 0.0f;  // Reset throttle
        
        // Apply immediate environmental effects
        ApplyEffectsToEntitiesInZone(zoneId, *climate);
    }
}

void WorldClimateSystem::OnPlayerExitedZone(const std::string& zoneId) {
    auto* climate = GetZoneClimate(zoneId);
    if (climate) {
        if (climate->playerCount > 0) {
            climate->playerCount--;
        }
        if (climate->playerCount == 0) {
            climate->hasActivePlayers = false;
        }
    }
}

void WorldClimateSystem::ApplyEffectsToEntitiesInZone(const std::string& zoneId, 
                                                       const ZoneClimateComponent& climate) {
    // Find all entities in this zone
    for (EntityID entity : ctx.registry->view<PositionComponent>()) {
        auto* pos = ctx.registry->GetComponent<PositionComponent>(entity);
        auto* region = ctx.registry->GetComponent<RegionComponent>(entity);
        
        if (!pos || !region) continue;
        
        // Check if entity is in this zone
        if (region->region != zoneId) continue;
        
        // Apply effects
        ApplyEnvironmentalEffects(entity, zoneId);
    }
}

void WorldClimateSystem::ApplyEnvironmentalEffects(int entityId, const std::string& zoneId) {
    auto* climate = GetZoneClimate(zoneId);
    if (!climate) return;
    
    // Get or create environmental effects component
    auto* effects = ctx.registry->GetComponent<EnvironmentalEffectComponent>(entityId);
    if (!effects) {
        ctx.registry->AddComponent<EnvironmentalEffectComponent>(entityId, 
                                                                  EnvironmentalEffectComponent{});
        effects = ctx.registry->GetComponent<EnvironmentalEffectComponent>(entityId);
    }
    
    // Clear old effects
    effects->Clear();
    
    // Apply time-based effects
    switch (climate->currentPhase) {
        case TimePhase::NIGHT:
            effects->AddEffect({ClimateEffect::Type::STEALTH_BONUS, 20, -1.0f, "night"});
            effects->AddEffect({ClimateEffect::Type::ACCURACY_PENALTY, -5, -1.0f, "night"});
            break;
        case TimePhase::DAWN:
        case TimePhase::DUSK:
            // Transitional - minor effects
            break;
        default:
            break;
    }
    
    // Apply weather-based effects
    switch (climate->currentWeather) {
        case WeatherCondition::HEAVY_RAIN:
            effects->AddEffect({ClimateEffect::Type::FIRE_PENALTY, -25, -1.0f, "rain"});
            effects->AddEffect({ClimateEffect::Type::MOVEMENT_PENALTY, -15, -1.0f, "rain"});
            effects->AddEffect({ClimateEffect::Type::ACCURACY_PENALTY, -10, -1.0f, "rain"});
            break;
        case WeatherCondition::STORM:
            effects->AddEffect({ClimateEffect::Type::FIRE_PENALTY, -40, -1.0f, "storm"});
            effects->AddEffect({ClimateEffect::Type::MOVEMENT_PENALTY, -20, -1.0f, "storm"});
            effects->AddEffect({ClimateEffect::Type::ACCURACY_PENALTY, -15, -1.0f, "storm"});
            effects->AddEffect({ClimateEffect::Type::STEALTH_PENALTY, -20, -1.0f, "storm"});
            break;
        case WeatherCondition::FOG:
            effects->AddEffect({ClimateEffect::Type::STEALTH_BONUS, 30, -1.0f, "fog"});
            effects->AddEffect({ClimateEffect::Type::ACCURACY_PENALTY, -20, -1.0f, "fog"});
            effects->AddEffect({ClimateEffect::Type::MOVEMENT_PENALTY, -20, -1.0f, "fog"});
            break;
        case WeatherCondition::SNOW:
            effects->AddEffect({ClimateEffect::Type::MOVEMENT_PENALTY, -25, -1.0f, "snow"});
            effects->AddEffect({ClimateEffect::Type::ACCURACY_PENALTY, -10, -1.0f, "snow"});
            break;
        default:
            break;
    }
}

std::string WorldClimateSystem::GetTimeString(const std::string& zoneId) {
    auto* climate = GetZoneClimate(zoneId);
    if (!climate) return "Unknown time";
    
    std::string hourStr;
    if (climate->gameHour == 0) hourStr = "midnight";
    else if (climate->gameHour == 12) hourStr = "noon";
    else if (climate->gameHour < 12) hourStr = std::to_string(climate->gameHour) + " AM";
    else hourStr = std::to_string(climate->gameHour - 12) + " PM";
    
    return "It is " + hourStr + " on day " + std::to_string(climate->gameDay) + 
           " of " + ClimateUtils::SeasonToString(static_cast<Season>(climate->gameMonth)) + 
           ", year " + std::to_string(climate->gameYear);
}

std::string WorldClimateSystem::GetWeatherString(const std::string& zoneId) {
    auto* climate = GetZoneClimate(zoneId);
    if (!climate) return "Unknown weather";
    
    std::string result = "The weather is " + 
                         std::string(ClimateUtils::WeatherToString(climate->currentWeather)) + ".\n";
    result += "Temperature: " + std::to_string(climate->temperature) + "F\n";
    result += "Wind: " + std::to_string(climate->windSpeed) + " mph from the " + 
              std::to_string(static_cast<int>(climate->windDir)) + "\n";
    
    if (climate->visibility < 255) {
        result += "Visibility: " + std::to_string(climate->visibility) + " feet\n";
    }
    
    return result;
}

void WorldClimateSystem::SetWeather(const std::string& zoneId, WeatherCondition weather) {
    auto* climate = GetZoneClimate(zoneId);
    if (!climate) return;
    
    WeatherCondition oldWeather = climate->currentWeather;
    climate->currentWeather = weather;
    climate->temperature = CalculateTemperature(*climate);
    climate->visibility = CalculateVisibility(*climate);
    
    BroadcastWeatherChange(zoneId, oldWeather, weather);
}

void WorldClimateSystem::SetTime(const std::string& zoneId, uint8_t hour, uint8_t day, uint8_t month) {
    auto* climate = GetZoneClimate(zoneId);
    if (!climate) return;
    
    TimePhase oldPhase = climate->currentPhase;
    
    climate->gameHour = hour % 24;
    climate->gameDay = static_cast<uint8_t>((std::max)(1, (std::min)(30, static_cast<int>(day))));
    climate->gameMonth = month % 4;
    climate->currentPhase = ClimateUtils::HourToPhase(climate->gameHour);
    
    if (climate->currentPhase != oldPhase) {
        BroadcastTimePhaseChange(zoneId, oldPhase, climate->currentPhase);
    }
}

std::vector<std::string> WorldClimateSystem::GetActiveZones() const {
    std::vector<std::string> activeZones;
    
    for (EntityID entity : ctx.registry->view<ZoneClimateComponent>()) {
        auto* climate = ctx.registry->GetComponent<ZoneClimateComponent>(entity);
        if (climate && climate->hasActivePlayers) {
            activeZones.push_back(climate->zoneId);
        }
    }
    
    return activeZones;
}

void WorldClimateSystem::AdvanceSeason(ZoneClimateComponent& climate) {
    // Season just changed - could trigger events
    Season newSeason = static_cast<Season>(climate.gameMonth);
    
    ctx.scripts->execute_hook("on_season_changed", climate.zoneId, 
                               static_cast<int>(newSeason));
}

void WorldClimateSystem::BroadcastWeatherChange(const std::string& zoneId, 
                                                 WeatherCondition oldWeather, 
                                                 WeatherCondition newWeather) {
    std::string msg = GenerateWeatherTransitionMessage(oldWeather, newWeather);
    
    // Send to all players in zone
    for (EntityID entity : ctx.registry->view<ClientComponent>()) {
        auto* client = ctx.registry->GetComponent<ClientComponent>(entity);
        auto* pos = ctx.registry->GetComponent<PositionComponent>(entity);
        auto* region = ctx.registry->GetComponent<RegionComponent>(entity);
        
        if (client && pos && region && region->region == zoneId) {
            // TODO: Send message via client->SendMessage or similar
        }
    }
}

void WorldClimateSystem::BroadcastTimePhaseChange(const std::string& zoneId, 
                                                   TimePhase oldPhase, 
                                                   TimePhase newPhase) {
    std::string msg = GenerateTimePhaseMessage(newPhase);
    
    // Send to all players in zone
    for (EntityID entity : ctx.registry->view<ClientComponent>()) {
        auto* client = ctx.registry->GetComponent<ClientComponent>(entity);
        auto* pos = ctx.registry->GetComponent<PositionComponent>(entity);
        auto* region = ctx.registry->GetComponent<RegionComponent>(entity);
        
        if (client && pos && region && region->region == zoneId) {
            // TODO: Send message via client->SendMessage or similar
        }
    }
}

std::string WorldClimateSystem::GenerateWeatherTransitionMessage(WeatherCondition from, 
                                                                  WeatherCondition to) {
    if (from == to) return "";
    
    switch (to) {
        case WeatherCondition::CLEAR:
            return "The clouds part and sunlight breaks through.";
        case WeatherCondition::PARTLY_CLOUDY:
            return "A few clouds drift across the sky.";
        case WeatherCondition::CLOUDY:
            return "Clouds begin to gather overhead.";
        case WeatherCondition::OVERCAST:
            return "The sky darkens as heavy clouds obscure the sun.";
        case WeatherCondition::LIGHT_RAIN:
            return "A light rain begins to fall.";
        case WeatherCondition::HEAVY_RAIN:
            return "The rain intensifies, coming down in sheets.";
        case WeatherCondition::STORM:
            return "Thunder rumbles as a storm rolls in!";
        case WeatherCondition::FOG:
            return "A thick fog begins to settle in.";
        case WeatherCondition::SNOW:
            return "Snowflakes begin to drift down from the sky.";
        default:
            return "";
    }
}

std::string WorldClimateSystem::GenerateTimePhaseMessage(TimePhase phase) {
    switch (phase) {
        case TimePhase::DAWN:
            return "The first light of dawn breaks across the horizon.";
        case TimePhase::MORNING:
            return "Morning has arrived.";
        case TimePhase::AFTERNOON:
            return "The sun reaches its peak in the afternoon sky.";
        case TimePhase::DUSK:
            return "The sun begins to set, painting the sky in hues of gold.";
        case TimePhase::EVENING:
            return "Evening settles in as darkness falls.";
        case TimePhase::NIGHT:
            return "Night has fallen. Stars appear in the darkened sky.";
        default:
            return "";
    }
}

// Simple random number generation
float WorldClimateSystem::RandomFloat() {
    return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

int WorldClimateSystem::RandomInt(int min, int max) {
    return min + (rand() % (max - min + 1));
}
