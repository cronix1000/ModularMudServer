-- ambient_ai.lua
-- Handles non-combat NPC behaviors like wandering, patrolling, and schedules

ambient_ai = {
    entities = {},
    update_interval = 1.0,  -- Update every second
    last_update = 0
}

-- Behavior types
BEHAVIOR = {
    IDLE = 0,
    WANDER = 1,
    PATROL = 2,
    SCHEDULED = 3,
    GUARD = 4,
    MERCHANT = 5
}

-- Activity states
STATE = {
    NONE = 0,
    IDLE = 1,
    MOVING = 2,
    INTERACTING = 3,
    SLEEPING = 4,
    WORKING = 5
}

-- Activity patterns for time-sensitive mobs
ACTIVITY_PATTERN = {
    ALWAYS_ACTIVE = 0,
    DIURNAL = 1,      -- Active day, sleep night
    NOCTURNAL = 2,    -- Active night, sleep day
    CREPUSCULAR = 3,  -- Active dawn/dusk
    SCHEDULED = 4
}

-- Time phases (match C++ enum)
TIME_PHASE = {
    NIGHT = 0,
    DAWN = 1,
    MORNING = 2,
    AFTERNOON = 3,
    DUSK = 4,
    EVENING = 5
}

-- Ambient emotes for NPCs
AMBIENT_EMOTES = {
    generic = {
        "looks around",
        "shifts from foot to foot",
        "checks the surroundings",
        "adjusts their clothing",
        "takes a deep breath",
        "glances at the sky"
    },
    merchant = {
        "arranges their wares",
        "calls out to passersby",
        "counts their coins",
        "polishes their goods",
        "waits patiently for customers"
    },
    guard = {
        "stands at attention",
        "scans the area vigilantly",
        "adjusts their grip on their weapon",
        "shifts their weight",
        "glances down the street"
    }
}

-- Initialize an entity for ambient AI
function setup_ambient_ai(entity_id, behavior_type)
    ambient_ai.entities[entity_id] = {
        id = entity_id,
        behavior = behavior_type,
        state = STATE.IDLE,
        last_update = 0,
        is_active = true,
        zone_id = nil,
        
        -- Wandering parameters
        home_room = -1,
        home_x = 0,
        home_y = 0,
        wander_radius = 5,
        wander_cooldown = 30.0,
        last_move_time = 0,
        wander_chance = 0.4,
        
        -- Patrol parameters
        waypoints = {},
        current_waypoint = 0,
        patrol_loop = true,
        patrol_forward = true,
        
        -- Guard parameters
        detection_radius = 2,
        target_faction = "",
        
        -- Merchant parameters
        shout_cooldown = 60.0,
        last_shout_time = 0,
        shout_messages = {},
        current_shout = 0,
        
        -- Emote parameters
        emote_cooldown = 45.0,
        last_emote_time = 0,
        emote_chance = 0.3,
        
        -- Time sensitivity
        activity_pattern = ACTIVITY_PATTERN.ALWAYS_ACTIVE,
        wake_hour = 6,
        sleep_hour = 22,
        is_asleep = false
    }
end

-- Main update function called from C++
function update_ambient_ai(entity_id, delta_time)
    local ai = ambient_ai.entities[entity_id]
    if not ai or not ai.is_active then return end
    
    ai.last_update = ai.last_update + delta_time
    
    -- Check behavior type and update accordingly
    if ai.behavior == BEHAVIOR.WANDER then
        update_wander_behavior(ai, delta_time)
    elseif ai.behavior == BEHAVIOR.PATROL then
        update_patrol_behavior(ai, delta_time)
    elseif ai.behavior == BEHAVIOR.GUARD then
        update_guard_behavior(ai, delta_time)
    elseif ai.behavior == BEHAVIOR.MERCHANT then
        update_merchant_behavior(ai, delta_time)
    elseif ai.behavior == BEHAVIOR.SCHEDULED then
        update_scheduled_behavior(ai, delta_time)
    end
    
    -- Try ambient emotes
    try_ambient_emote(ai, delta_time)
end

function update_wander_behavior(ai, delta_time)
    -- Check if it's time to move
    local current_time = get_game_time()
    if current_time - ai.last_move_time < ai.wander_cooldown then
        return
    end
    
    -- Roll for movement
    if math.random() > ai.wander_chance then
        ai.last_move_time = current_time  -- Reset cooldown even if not moving
        return
    end
    
    -- Calculate new position within radius
    local new_x = ai.home_x + math.random(-ai.wander_radius, ai.wander_radius)
    local new_y = ai.home_y + math.random(-ai.wander_radius, ai.wander_radius)
    
    -- Check if valid move
    if can_move_to(ai.home_room, new_x, new_y) then
        move_entity(ai.id, ai.home_room, new_x, new_y)
        ai.last_move_time = current_time
        
        -- Small chance to emote after moving
        if math.random() < 0.2 then
            send_emote(ai.id, "wanders " .. get_direction_string(ai.home_x, ai.home_y, new_x, new_y))
        end
    end
end

function update_patrol_behavior(ai, delta_time)
    if #ai.waypoints == 0 then return end
    
    local current_time = get_game_time()
    local waypoint = ai.waypoints[ai.current_waypoint + 1]  -- Lua is 1-indexed
    
    if not waypoint then return end
    
    -- Check if we're at the waypoint
    if is_at_position(ai.id, waypoint.room_id, waypoint.x, waypoint.y) then
        -- Pause at waypoint
        if current_time - ai.last_move_time < waypoint.pause_time then
            return
        end
        
        -- Advance to next waypoint
        advance_waypoint(ai)
        ai.last_move_time = current_time
    else
        -- Move toward waypoint
        move_toward(ai.id, waypoint.room_id, waypoint.x, waypoint.y)
    end
end

function advance_waypoint(ai)
    if ai.patrol_loop then
        -- Loop mode: wrap around
        ai.current_waypoint = (ai.current_waypoint + 1) % #ai.waypoints
    else
        -- Ping-pong mode: bounce back and forth
        if ai.patrol_forward then
            ai.current_waypoint = ai.current_waypoint + 1
            if ai.current_waypoint >= #ai.waypoints - 1 then
                ai.patrol_forward = false
            end
        else
            ai.current_waypoint = ai.current_waypoint - 1
            if ai.current_waypoint <= 0 then
                ai.patrol_forward = true
            end
        end
    end
end

function update_guard_behavior(ai, delta_time)
    -- Scan for targets in detection radius
    local target = find_entity_in_radius(ai.id, ai.detection_radius, function(entity)
        return has_faction(entity, ai.target_faction)
    end)
    
    if target then
        -- Found a target!
        if math.random() < 0.7 then
            -- Confront
            send_emote(ai.id, "spots someone suspicious and moves to investigate!")
            trigger_guard_confrontation(ai.id, target)
        else
            -- Just watch
            send_emote(ai.id, "eyes someone suspiciously.")
        end
    end
end

function update_merchant_behavior(ai, delta_time)
    local current_time = get_game_time()
    
    -- Check if it's time to shout
    if current_time - ai.last_shout_time >= ai.shout_cooldown then
        if #ai.shout_messages > 0 then
            local msg = ai.shout_messages[ai.current_shout + 1]
            broadcast_to_room(get_entity_room(ai.id), "A merchant calls out: \"" .. msg .. "\"")
            
            ai.current_shout = (ai.current_shout + 1) % #ai.shout_messages
            ai.last_shout_time = current_time
        end
    end
    
    -- Try merchant-specific emotes
    if math.random() < 0.1 then
        local emotes = AMBIENT_EMOTES.merchant
        local emote = emotes[math.random(1, #emotes)]
        send_emote(ai.id, emote)
    end
end

function update_scheduled_behavior(ai, delta_time)
    -- Scheduled behavior updates based on current activity
    -- This would integrate with NPCScheduleComponent
    -- For now, simplified version
end

function update_time_sensitive(entity_id, hour)
    local ai = ambient_ai.entities[entity_id]
    if not ai then return end
    
    local should_be_awake = true
    
    if ai.activity_pattern == ACTIVITY_PATTERN.DIURNAL then
        should_be_awake = (hour >= ai.wake_hour and hour < ai.sleep_hour)
    elseif ai.activity_pattern == ACTIVITY_PATTERN.NOCTURNAL then
        should_be_awake = (hour >= ai.sleep_hour or hour < ai.wake_hour)
    elseif ai.activity_pattern == ACTIVITY_PATTERN.CREPUSCULAR then
        -- Active at dawn (4-7) and dusk (16-19)
        should_be_awake = (hour >= 4 and hour < 7) or (hour >= 16 and hour < 19)
    end
    
    if should_be_awake and ai.is_asleep then
        -- Wake up
        ai.is_asleep = false
        ai.is_active = true
        send_emote(entity_id, "awakens and stretches.")
    elseif not should_be_awake and not ai.is_asleep then
        -- Go to sleep
        ai.is_asleep = true
        ai.is_active = false
        send_emote(entity_id, "settles down to sleep.")
    end
end

function try_ambient_emote(ai, delta_time)
    local current_time = get_game_time()
    
    if current_time - ai.last_emote_time < ai.emote_cooldown then
        return
    end
    
    if math.random() > ai.emote_chance then
        ai.last_emote_time = current_time
        return
    end
    
    -- Select emote based on behavior type
    local emote_pool = AMBIENT_EMOTES.generic
    if ai.behavior == BEHAVIOR.MERCHANT then
        emote_pool = AMBIENT_EMOTES.merchant
    elseif ai.behavior == BEHAVIOR.GUARD then
        emote_pool = AMBIENT_EMOTES.guard
    end
    
    local emote = emote_pool[math.random(1, #emote_pool)]
    send_emote(ai.id, emote)
    ai.last_emote_time = current_time
end

-- Setup helpers
function setup_wander(entity_id, home_room, home_x, home_y, radius, cooldown)
    local ai = ambient_ai.entities[entity_id]
    if not ai then
        setup_ambient_ai(entity_id, BEHAVIOR.WANDER)
        ai = ambient_ai.entities[entity_id]
    end
    
    ai.behavior = BEHAVIOR.WANDER
    ai.home_room = home_room
    ai.home_x = home_x
    ai.home_y = home_y
    ai.wander_radius = radius or 5
    ai.wander_cooldown = cooldown or 30.0
end

function setup_patrol(entity_id, waypoints, loop)
    local ai = ambient_ai.entities[entity_id]
    if not ai then
        setup_ambient_ai(entity_id, BEHAVIOR.PATROL)
        ai = ambient_ai.entities[entity_id]
    end
    
    ai.behavior = BEHAVIOR.PATROL
    ai.waypoints = waypoints or {}
    ai.patrol_loop = loop ~= false  -- Default true
    ai.current_waypoint = 0
end

function setup_guard(entity_id, detection_radius, target_faction)
    local ai = ambient_ai.entities[entity_id]
    if not ai then
        setup_ambient_ai(entity_id, BEHAVIOR.GUARD)
        ai = ambient_ai.entities[entity_id]
    end
    
    ai.behavior = BEHAVIOR.GUARD
    ai.detection_radius = detection_radius or 2
    ai.target_faction = target_faction or ""
end

function setup_merchant(entity_id, shout_cooldown, messages)
    local ai = ambient_ai.entities[entity_id]
    if not ai then
        setup_ambient_ai(entity_id, BEHAVIOR.MERCHANT)
        ai = ambient_ai.entities[entity_id]
    end
    
    ai.behavior = BEHAVIOR.MERCHANT
    ai.shout_cooldown = shout_cooldown or 60.0
    ai.shout_messages = messages or {}
end

function setup_time_sensitive(entity_id, pattern, wake_hour, sleep_hour)
    local ai = ambient_ai.entities[entity_id]
    if not ai then
        setup_ambient_ai(entity_id, BEHAVIOR.IDLE)
        ai = ambient_ai.entities[entity_id]
    end
    
    ai.activity_pattern = pattern or ACTIVITY_PATTERN.ALWAYS_ACTIVE
    ai.wake_hour = wake_hour or 6
    ai.sleep_hour = sleep_hour or 22
end

-- Zone tracking
function on_player_entered_zone(zone_id)
    -- Activate all entities in this zone
    for entity_id, ai in pairs(ambient_ai.entities) do
        if ai.zone_id == zone_id then
            ai.is_active = true
        end
    end
end

function on_player_exited_zone(zone_id)
    -- Deactivate entities in this zone (except important ones)
    for entity_id, ai in pairs(ambient_ai.entities) do
        if ai.zone_id == zone_id then
            -- Keep guards active, deactivate others
            if ai.behavior ~= BEHAVIOR.GUARD then
                ai.is_active = false
            end
        end
    end
end

-- Helper functions
function get_direction_string(from_x, from_y, to_x, to_y)
    local dx = to_x - from_x
    local dy = to_y - from_y
    
    if math.abs(dx) > math.abs(dy) then
        if dx > 0 then return "east"
        else return "west" end
    else
        if dy > 0 then return "south"
        else return "north" end
    end
end

-- Stub functions - these would be implemented in C++ and exposed to Lua
function get_game_time() return 0 end
function can_move_to(room, x, y) return true end
function move_entity(entity, room, x, y) end
function is_at_position(entity, room, x, y) return false end
function move_toward(entity, room, x, y) end
function find_entity_in_radius(entity, radius, predicate) return nil end
function has_faction(entity, faction) return false end
function get_entity_room(entity) return -1 end
function send_emote(entity, message) end
function broadcast_to_room(room, message) end
function trigger_guard_confrontation(guard, target) end

print("Ambient AI system loaded successfully!")
