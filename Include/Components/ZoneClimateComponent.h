#pragma once
#include <string>
#include <cstdint>

// Weather condition types - using enum for fast comparison and low memory
enum class WeatherCondition : uint8_t {
    CLEAR = 0,
    PARTLY_CLOUDY,
    CLOUDY,
    OVERCAST,
    LIGHT_RAIN,
    HEAVY_RAIN,
    STORM,
    FOG,
    SNOW,
    COUNT  // Keep last for array sizing
};

// Time of day phases
enum class TimePhase : uint8_t {
    NIGHT = 0,   // 0-4 hours
    DAWN,        // 4-6 hours
    MORNING,     // 6-12 hours
    AFTERNOON,   // 12-16 hours
    DUSK,        // 16-18 hours
    EVENING      // 18-24 hours
};

// Seasons
enum class Season : uint8_t {
    SPRING = 0,
    SUMMER,
    AUTUMN,
    WINTER
};

// Wind directions
enum class WindDirection : uint8_t {
    N = 0, NE, E, SE, S, SW, W, NW
};

/**
 * ZoneClimateComponent - Per-region/floor weather and time state
 * Attached to a single "climate controller" entity per zone
 * This allows different floors to have different weather/time
 */
struct ZoneClimateComponent {
    // Time tracking (game time, not real time)
    uint8_t gameHour;           // 0-23
    uint8_t gameDay;            // 1-30 (per season)
    uint8_t gameMonth;          // 0-3 (Spring, Summer, Autumn, Winter)
    uint16_t gameYear;          // Year counter
    float timeAccumulator;      // Fractional hours (for smooth transitions)
    
    // Cached time phase (updated when hour changes)
    TimePhase currentPhase;
    
    // Weather state
    WeatherCondition currentWeather;
    WeatherCondition targetWeather;    // For smooth transitions
    float weatherTransition;           // 0.0 to 1.0 during change
    
    // Temperature in Fahrenheit (-50 to 150 typical range)
    int8_t temperature;
    
    // Wind
    uint8_t windSpeed;          // 0-255 (mapped to 0-100+ mph)
    WindDirection windDir;
    
    // Visibility (feet, 255 = unlimited)
    uint8_t visibility;
    
    // Zone identification
    std::string zoneId;         // Matches RegionComponent::region
    std::string zoneName;       // Display name
    
    // Performance optimization: only update if players present
    bool hasActivePlayers;
    uint32_t playerCount;
    float timeSinceLastUpdate;  // Throttle updates for empty zones
    
    // Climate profile (for different zone types)
    enum class ClimateType : uint8_t {
        TEMPERATE,      // Standard 4 seasons
        TROPICAL,       // Warm, wet seasons
        ARID,           // Hot, dry
        ARCTIC,         // Cold, snow
        UNDERGROUND,    // No weather, artificial light
        MAGICAL         // Special rules
    } climateType;
    
    // Indoor/outdoor flag (indoor zones don't get weather)
    bool isOutdoor;
    
    // Constructor with defaults
    ZoneClimateComponent() 
        : gameHour(12), gameDay(1), gameMonth(0), gameYear(1)
        , timeAccumulator(0.0f), currentPhase(TimePhase::AFTERNOON)
        , currentWeather(WeatherCondition::CLEAR), targetWeather(WeatherCondition::CLEAR)
        , weatherTransition(0.0f), temperature(70), windSpeed(5)
        , windDir(WindDirection::S), visibility(255)
        , hasActivePlayers(false), playerCount(0), timeSinceLastUpdate(0.0f)
        , climateType(ClimateType::TEMPERATE), isOutdoor(true) {}
};

/**
 * ClimateEffect - Active environmental effect on an entity
 * Lightweight struct for batch processing
 */
struct ClimateEffect {
    enum class Type : uint8_t {
        STEALTH_BONUS,
        STEALTH_PENALTY,
        FIRE_PENALTY,
        FIRE_BONUS,
        MOVEMENT_PENALTY,
        MOVEMENT_BONUS,
        ACCURACY_PENALTY,
        ACCURACY_BONUS
    } type;
    
    int8_t magnitude;           // Percentage (-100 to +100)
    float duration;             // Seconds remaining
    std::string source;         // "night", "rain", "storm", etc.
};

/**
 * EnvironmentalEffectComponent - Active climate effects on an entity
 * Applied by WorldClimateSystem based on zone conditions
 */
struct EnvironmentalEffectComponent {
    static constexpr size_t MAX_EFFECTS = 8;  // Fixed size for performance
    ClimateEffect effects[MAX_EFFECTS];
    uint8_t effectCount;
    
    EnvironmentalEffectComponent() : effectCount(0) {}
    
    // Helper to add effect (returns false if full)
    bool AddEffect(const ClimateEffect& effect) {
        if (effectCount >= MAX_EFFECTS) return false;
        effects[effectCount++] = effect;
        return true;
    }
    
    // Helper to remove effect at index
    void RemoveEffect(uint8_t index) {
        if (index >= effectCount) return;
        // Shift remaining effects down
        for (uint8_t i = index; i < effectCount - 1; ++i) {
            effects[i] = effects[i + 1];
        }
        --effectCount;
    }
    
    // Clear all effects
    void Clear() { effectCount = 0; }
};

/**
 * Helper functions for time/weather calculations
 */
namespace ClimateUtils {
    // Convert game hour to time phase
    inline TimePhase HourToPhase(uint8_t hour) {
        if (hour < 4) return TimePhase::NIGHT;
        if (hour < 6) return TimePhase::DAWN;
        if (hour < 12) return TimePhase::MORNING;
        if (hour < 16) return TimePhase::AFTERNOON;
        if (hour < 18) return TimePhase::DUSK;
        return TimePhase::EVENING;
    }
    
    // Get phase name for display
    inline const char* PhaseToString(TimePhase phase) {
        switch (phase) {
            case TimePhase::NIGHT: return "Night";
            case TimePhase::DAWN: return "Dawn";
            case TimePhase::MORNING: return "Morning";
            case TimePhase::AFTERNOON: return "Afternoon";
            case TimePhase::DUSK: return "Dusk";
            case TimePhase::EVENING: return "Evening";
        }
        return "Unknown";
    }
    
    // Get weather name for display
    inline const char* WeatherToString(WeatherCondition weather) {
        switch (weather) {
            case WeatherCondition::CLEAR: return "Clear";
            case WeatherCondition::PARTLY_CLOUDY: return "Partly Cloudy";
            case WeatherCondition::CLOUDY: return "Cloudy";
            case WeatherCondition::OVERCAST: return "Overcast";
            case WeatherCondition::LIGHT_RAIN: return "Light Rain";
            case WeatherCondition::HEAVY_RAIN: return "Heavy Rain";
            case WeatherCondition::STORM: return "Storm";
            case WeatherCondition::FOG: return "Fog";
            case WeatherCondition::SNOW: return "Snow";
            default: return "Unknown";
        }
    }
    
    // Get season name
    inline const char* SeasonToString(Season season) {
        switch (season) {
            case Season::SPRING: return "Spring";
            case Season::SUMMER: return "Summer";
            case Season::AUTUMN: return "Autumn";
            case Season::WINTER: return "Winter";
        }
        return "Unknown";
    }
    
    // Calculate base temperature from season and time
    inline int8_t CalculateBaseTemperature(Season season, TimePhase phase) {
        int8_t base = 60;  // Base temperate
        
        // Season modifiers
        switch (season) {
            case Season::SPRING: base = 55; break;
            case Season::SUMMER: base = 80; break;
            case Season::AUTUMN: base = 50; break;
            case Season::WINTER: base = 30; break;
        }
        
        // Time of day modifiers
        switch (phase) {
            case TimePhase::NIGHT: base -= 10; break;
            case TimePhase::DAWN: base -= 5; break;
            case TimePhase::MORNING: base += 5; break;
            case TimePhase::AFTERNOON: base += 10; break;
            case TimePhase::DUSK: base += 5; break;
            case TimePhase::EVENING: base -= 5; break;
        }
        
        return base;
    }
}
