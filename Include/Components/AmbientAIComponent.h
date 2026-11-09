#pragma once
#include <vector>
#include <string>
#include <cstdint>

/**
 * AmbientAIComponent - Controls non-combat NPC behaviors
 * Wandering, patrolling, and scheduled activities
 */
struct AmbientAIComponent {
    // Behavior types
    enum class BehaviorType : uint8_t {
        IDLE = 0,       // Stands still, maybe turns
        WANDER,         // Random movement within radius
        PATROL,         // Follows waypoints
        SCHEDULED,      // Time-based activity changes
        GUARD,          // Watches for specific entities
        MERCHANT        // Stays near shop, calls out wares
    };
    
    BehaviorType behavior;
    
    // Current state
    enum class State : uint8_t {
        NONE = 0,
        IDLE,
        MOVING,
        INTERACTING,
        SLEEPING,
        WORKING
    } currentState;
    
    // === WANDER PARAMETERS ===
    int homeRoomId;             // Room to wander around
    int homeX, homeY;          // Center point
    uint8_t wanderRadius;       // Max distance from home (in rooms)
    float wanderCooldown;       // Seconds between moves
    float lastMoveTime;         // Timestamp of last move
    float wanderChance;         // 0.0-1.0 chance to move when off cooldown
    
    // === PATROL PARAMETERS ===
    struct Waypoint {
        int roomId;
        int x, y;
        float pauseTime;        // Seconds to pause at this point
    };
    static constexpr size_t MAX_WAYPOINTS = 8;
    Waypoint waypoints[MAX_WAYPOINTS];
    uint8_t waypointCount;
    uint8_t currentWaypoint;
    bool patrolLoop;            // true = loop, false = ping-pong
    bool patrolForward;         // For ping-pong: true = forward, false = backward
    
    // === SCHEDULE PARAMETERS ===
    struct ScheduleEntry {
        uint8_t startHour;      // Game hour to start (0-23)
        uint8_t endHour;        // Game hour to end (0-23)
        State activity;         // What to do during this time
        int targetRoomId;       // Where to do it
        int targetX, targetY;
    };
    static constexpr size_t MAX_SCHEDULES = 4;
    ScheduleEntry schedule[MAX_SCHEDULES];
    uint8_t scheduleCount;
    uint8_t currentSchedule;    // Which schedule entry is active
    
    // === GUARD PARAMETERS ===
    uint8_t detectionRadius;    // Rooms away to detect
    float confrontationChance;  // Chance to confront vs just watch
    std::string targetFaction;  // Who to watch for
    
    // === MERCHANT PARAMETERS ===
    float shoutCooldown;        // Seconds between calling out
    float lastShoutTime;
    std::vector<std::string> shoutMessages;  // What to shout
    uint8_t currentShoutIndex;
    
    // === EMOTE SYSTEM ===
    float emoteCooldown;        // Seconds between ambient emotes
    float lastEmoteTime;
    float emoteChance;          // Chance to emote when cooldown expires
    std::vector<std::string> ambientEmotes;  // "looks around", "yawns", etc.
    
    // === MOVEMENT STATE ===
    bool isMoving;
    int targetRoomId;
    int targetX, targetY;
    float moveProgress;         // 0.0 to 1.0 (for smooth movement display)
    
    // Performance: only update if in active zone
    bool isActive;
    float timeSinceUpdate;
    
    // Constructor
    AmbientAIComponent() 
        : behavior(BehaviorType::IDLE)
        , currentState(State::IDLE)
        , homeRoomId(-1), homeX(0), homeY(0)
        , wanderRadius(3), wanderCooldown(30.0f), lastMoveTime(0.0f), wanderChance(0.4f)
        , waypointCount(0), currentWaypoint(0)
        , patrolLoop(true), patrolForward(true)
        , scheduleCount(0), currentSchedule(0)
        , detectionRadius(2), confrontationChance(0.7f)
        , shoutCooldown(60.0f), lastShoutTime(0.0f), currentShoutIndex(0)
        , emoteCooldown(45.0f), lastEmoteTime(0.0f), emoteChance(0.3f)
        , isMoving(false), targetRoomId(-1), targetX(0), targetY(0), moveProgress(0.0f)
        , isActive(true), timeSinceUpdate(0.0f) {}
};

/**
 * TimeSensitiveComponent - For mobs that behave differently at different times
 * Nocturnal, diurnal, crepuscular creatures
 */
struct TimeSensitiveComponent {
    enum class ActivityPattern : uint8_t {
        ALWAYS_ACTIVE = 0,      // No time-based changes
        DIURNAL,                // Active day, sleeps at night
        NOCTURNAL,              // Active night, sleeps during day
        CREPUSCULAR,            // Active at dawn/dusk
        SCHEDULED               // Complex schedule
    } pattern;
    
    // Sleep/wake hours (for DIURNAL/NOCTURNAL)
    uint8_t wakeHour;
    uint8_t sleepHour;
    
    // Current state
    bool isAsleep;
    bool wasActive;             // Track state changes
    
    // Aggression changes based on time
    bool aggressiveWhenActive;
    bool aggressiveWhenSleeping;
    
    // Position to return to when sleeping
    int sleepRoomId;
    int sleepX, sleepY;
    
    // Messages
    std::string wakeMessage;    // "The wolf awakens and sniffs the air."
    std::string sleepMessage;   // "The wolf curls up and falls asleep."
    
    // For SCHEDULED pattern
    struct TimeBlock {
        uint8_t startHour;
        uint8_t endHour;
        bool isActive;
        bool isAggressive;
    };
    static constexpr size_t MAX_BLOCKS = 3;
    TimeBlock schedule[MAX_BLOCKS];
    uint8_t blockCount;
    
    // Constructor
    TimeSensitiveComponent()
        : pattern(ActivityPattern::ALWAYS_ACTIVE)
        , wakeHour(6), sleepHour(22)
        , isAsleep(false), wasActive(true)
        , aggressiveWhenActive(true), aggressiveWhenSleeping(false)
        , sleepRoomId(-1), sleepX(0), sleepY(0)
        , blockCount(0) {}
    
    // Helper: Check if should be awake at given hour
    bool ShouldBeAwake(uint8_t hour) const {
        switch (pattern) {
            case ActivityPattern::ALWAYS_ACTIVE:
                return true;
                
            case ActivityPattern::DIURNAL:
                return hour >= wakeHour && hour < sleepHour;
                
            case ActivityPattern::NOCTURNAL:
                return hour >= sleepHour || hour < wakeHour;
                
            case ActivityPattern::CREPUSCULAR:
                // Active at dawn (4-7) and dusk (16-19)
                return (hour >= 4 && hour < 7) || (hour >= 16 && hour < 19);
                
            case ActivityPattern::SCHEDULED:
                for (uint8_t i = 0; i < blockCount; ++i) {
                    if (hour >= schedule[i].startHour && hour < schedule[i].endHour) {
                        return schedule[i].isActive;
                    }
                }
                return true;  // Default to active if no schedule matches
        }
        return true;
    }
    
    // Helper: Get aggression state at given hour
    bool IsAggressiveAt(uint8_t hour) const {
        if (pattern == ActivityPattern::SCHEDULED) {
            for (uint8_t i = 0; i < blockCount; ++i) {
                if (hour >= schedule[i].startHour && hour < schedule[i].endHour) {
                    return schedule[i].isAggressive;
                }
            }
            return false;
        }
        
        bool awake = ShouldBeAwake(hour);
        return awake ? aggressiveWhenActive : aggressiveWhenSleeping;
    }
};

/**
 * NPCScheduleComponent - For complex daily routines
 * Can be attached alongside AmbientAIComponent
 */
struct NPCScheduleComponent {
    // What this NPC does during each time block
    struct Activity {
        uint8_t startHour;
        uint8_t endHour;
        std::string activityName;    // "working", "eating", "sleeping", "patrolling"
        int locationRoomId;
        int locationX, locationY;
        bool allowInterrupt;         // Can be interrupted by events?
    };
    
    static constexpr size_t MAX_ACTIVITIES = 6;
    Activity activities[MAX_ACTIVITIES];
    uint8_t activityCount;
    uint8_t currentActivity;
    
    // State tracking
    bool isAtActivityLocation;
    float activityStartTime;
    
    // Constructor
    NPCScheduleComponent() 
        : activityCount(0), currentActivity(0)
        , isAtActivityLocation(false), activityStartTime(0.0f) {}
    
    // Get current activity for hour
    const Activity* GetActivityForHour(uint8_t hour) const {
        for (uint8_t i = 0; i < activityCount; ++i) {
            if (hour >= activities[i].startHour && hour < activities[i].endHour) {
                return &activities[i];
            }
        }
        return nullptr;
    }
};
