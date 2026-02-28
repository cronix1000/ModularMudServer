-- climate_manager.lua
-- Manages weather and time across all zones
-- Provides hooks for C++ system to call

climate_manager = {
    zones = {},
    time_scale = 60,  -- 1 real second = 60 game seconds (1 game minute)
    last_update = 0
}

-- Weather condition constants
WEATHER = {
    CLEAR = 0,
    PARTLY_CLOUDY = 1,
    CLOUDY = 2,
    OVERCAST = 3,
    LIGHT_RAIN = 4,
    HEAVY_RAIN = 5,
    STORM = 6,
    FOG = 7,
    SNOW = 8
}

-- Time phase constants
TIME_PHASE = {
    NIGHT = 0,
    DAWN = 1,
    MORNING = 2,
    AFTERNOON = 3,
    DUSK = 4,
    EVENING = 5
}

-- Season constants
SEASON = {
    SPRING = 0,
    SUMMER = 1,
    AUTUMN = 2,
    WINTER = 3
}

-- Weather messages for players
WEATHER_MESSAGES = {
    [WEATHER.CLEAR] = "The sky is clear and bright.",
    [WEATHER.PARTLY_CLOUDY] = "A few clouds drift lazily across the sky.",
    [WEATHER.CLOUDY] = "Clouds gather overhead, obscuring the sun.",
    [WEATHER.OVERCAST] = "The sky is gray and overcast.",
    [WEATHER.LIGHT_RAIN] = "A light rain falls, pattering softly against the ground.",
    [WEATHER.HEAVY_RAIN] = "Rain pours down in heavy sheets.",
    [WEATHER.STORM] = "A fierce storm rages with thunder and lightning!",
    [WEATHER.FOG] = "A thick fog obscures your vision.",
    [WEATHER.SNOW] = "Snowflakes drift down from the gray sky."
}

-- Time phase messages
TIME_PHASE_MESSAGES = {
    [TIME_PHASE.NIGHT] = "It is the middle of the night.",
    [TIME_PHASE.DAWN] = "The first light of dawn breaks across the horizon.",
    [TIME_PHASE.MORNING] = "Morning has arrived with the rising sun.",
    [TIME_PHASE.AFTERNOON] = "The sun reaches its peak in the afternoon sky.",
    [TIME_PHASE.DUSK] = "The sun begins to set, painting the sky in gold and orange.",
    [TIME_PHASE.EVENING] = "Evening settles in as darkness falls."
}

-- Called when C++ creates a new climate zone
function on_zone_created(zone_id, zone_name, climate_type, is_outdoor)
    climate_manager.zones[zone_id] = {
        id = zone_id,
        name = zone_name,
        climate_type = climate_type,
        is_outdoor = is_outdoor,
        hour = 12,
        day = 1,
        month = 0,  -- Spring
        year = 1,
        weather = WEATHER.CLEAR,
        temperature = 70,
        wind_speed = 5,
        wind_dir = 4,  -- South
        visibility = 255,
        has_players = false,
        player_count = 0
    }
    print("Climate zone created: " .. zone_name .. " (" .. zone_id .. ")")
end

-- Called every game hour in C++
function on_hour_changed(zone_id, hour)
    local zone = climate_manager.zones[zone_id]
    if not zone then return end
    
    zone.hour = hour
    
    -- Calculate time phase
    local old_phase = get_time_phase(zone.hour - 1)
    local new_phase = get_time_phase(hour)
    
    -- Broadcast phase change if different
    if old_phase ~= new_phase and zone.has_players then
        broadcast_to_zone(zone_id, TIME_PHASE_MESSAGES[new_phase])
        on_time_phase_changed(zone_id, old_phase, new_phase)
    end
    
    -- Random weather change chance (only for outdoor zones)
    if zone.is_outdoor then
        attempt_weather_change(zone)
    end
end

-- Called when weather changes in C++
function on_weather_changed(zone_id, old_weather, new_weather)
    local zone = climate_manager.zones[zone_id]
    if not zone then return end
    
    zone.weather = new_weather
    
    -- Update derived properties
    zone.temperature = calculate_temperature(zone)
    zone.visibility = calculate_visibility(zone)
    
    -- Broadcast to players in zone
    if zone.has_players then
        local transition_msg = get_weather_transition_message(old_weather, new_weather)
        if transition_msg then
            broadcast_to_zone(zone_id, transition_msg)
        end
    end
    
    -- Fire Lua event for scripts to respond
    trigger_event("weather_changed", {
        zone_id = zone_id,
        old_weather = old_weather,
        new_weather = new_weather
    })
end

-- Called when season changes
function on_season_changed(zone_id, new_season)
    local zone = climate_manager.zones[zone_id]
    if not zone then return end
    
    zone.month = new_season
    
    local season_names = {"Spring", "Summer", "Autumn", "Winter"}
    if zone.has_players then
        broadcast_to_zone(zone_id, "The season changes to " .. season_names[new_season + 1] .. "!")
    end
end

-- Helper functions
function get_time_phase(hour)
    if hour < 4 then return TIME_PHASE.NIGHT end
    if hour < 6 then return TIME_PHASE.DAWN end
    if hour < 12 then return TIME_PHASE.MORNING end
    if hour < 16 then return TIME_PHASE.AFTERNOON end
    if hour < 18 then return TIME_PHASE.DUSK end
    return TIME_PHASE.EVENING
end

function attempt_weather_change(zone)
    -- Calculate change chance based on climate type and season
    local change_chance = 0.15  -- 15% base
    
    -- Season modifiers
    if zone.month == SEASON.AUTUMN then
        change_chance = 0.30  -- More changeable in autumn
    elseif zone.month == SEASON.SPRING then
        change_chance = 0.25
    elseif zone.month == SEASON.SUMMER then
        change_chance = 0.10
    end
    
    -- Roll for change
    if math.random() > change_chance then return end
    
    -- Determine new weather
    local new_weather = determine_next_weather(zone.weather)
    if new_weather ~= zone.weather then
        -- C++ will handle the actual change
        set_zone_weather(zone.id, new_weather)
    end
end

function determine_next_weather(current)
    local roll = math.random()
    
    -- Simple weather state machine
    if current == WEATHER.CLEAR then
        if roll < 0.70 then return WEATHER.CLEAR end
        if roll < 0.90 then return WEATHER.PARTLY_CLOUDY end
        return WEATHER.CLOUDY
    elseif current == WEATHER.PARTLY_CLOUDY then
        if roll < 0.40 then return WEATHER.CLEAR end
        if roll < 0.70 then return WEATHER.PARTLY_CLOUDY end
        if roll < 0.90 then return WEATHER.CLOUDY end
        return WEATHER.OVERCAST
    elseif current == WEATHER.CLOUDY then
        if roll < 0.30 then return WEATHER.PARTLY_CLOUDY end
        if roll < 0.60 then return WEATHER.CLOUDY end
        if roll < 0.80 then return WEATHER.OVERCAST end
        return WEATHER.LIGHT_RAIN
    elseif current == WEATHER.OVERCAST then
        if roll < 0.30 then return WEATHER.CLOUDY end
        if roll < 0.50 then return WEATHER.OVERCAST end
        if roll < 0.80 then return WEATHER.LIGHT_RAIN end
        return WEATHER.HEAVY_RAIN
    elseif current == WEATHER.LIGHT_RAIN then
        if roll < 0.20 then return WEATHER.CLOUDY end
        if roll < 0.40 then return WEATHER.OVERCAST end
        if roll < 0.70 then return WEATHER.LIGHT_RAIN end
        if roll < 0.90 then return WEATHER.HEAVY_RAIN end
        return WEATHER.STORM
    elseif current == WEATHER.HEAVY_RAIN then
        if roll < 0.20 then return WEATHER.LIGHT_RAIN end
        if roll < 0.50 then return WEATHER.HEAVY_RAIN end
        if roll < 0.80 then return WEATHER.STORM end
        return WEATHER.LIGHT_RAIN
    elseif current == WEATHER.STORM then
        if roll < 0.40 then return WEATHER.STORM end  -- Storms persist
        if roll < 0.70 then return WEATHER.HEAVY_RAIN end
        return WEATHER.LIGHT_RAIN
    elseif current == WEATHER.FOG then
        if roll < 0.60 then return WEATHER.CLEAR end
        return WEATHER.FOG
    elseif current == WEATHER.SNOW then
        if roll < 0.30 then return WEATHER.CLEAR end
        if roll < 0.70 then return WEATHER.SNOW end
        return WEATHER.LIGHT_RAIN
    end
    
    return WEATHER.CLEAR
end

function get_weather_transition_message(from, to)
    if from == to then return nil end
    
    if to == WEATHER.CLEAR then
        return "The clouds part and sunlight breaks through."
    elseif to == WEATHER.PARTLY_CLOUDY then
        return "A few clouds drift across the sky."
    elseif to == WEATHER.CLOUDY then
        return "Clouds begin to gather overhead."
    elseif to == WEATHER.OVERCAST then
        return "The sky darkens as heavy clouds obscure the sun."
    elseif to == WEATHER.LIGHT_RAIN then
        return "A light rain begins to fall."
    elseif to == WEATHER.HEAVY_RAIN then
        return "The rain intensifies, coming down in sheets."
    elseif to == WEATHER.STORM then
        return "Thunder rumbles as a storm rolls in!"
    elseif to == WEATHER.FOG then
        return "A thick fog begins to settle in."
    elseif to == WEATHER.SNOW then
        return "Snowflakes begin to drift down from the sky."
    end
    return nil
end

function calculate_temperature(zone)
    local base = 60
    
    -- Season
    if zone.month == SEASON.SPRING then base = 55
    elseif zone.month == SEASON.SUMMER then base = 80
    elseif zone.month == SEASON.AUTUMN then base = 50
    elseif zone.month == SEASON.WINTER then base = 30
    end
    
    -- Time of day
    local phase = get_time_phase(zone.hour)
    if phase == TIME_PHASE.NIGHT then base = base - 10
    elseif phase == TIME_PHASE.DAWN then base = base - 5
    elseif phase == TIME_PHASE.MORNING then base = base + 5
    elseif phase == TIME_PHASE.AFTERNOON then base = base + 10
    elseif phase == TIME_PHASE.DUSK then base = base + 5
    elseif phase == TIME_PHASE.EVENING then base = base - 5
    end
    
    -- Weather
    if zone.weather == WEATHER.CLEAR then base = base + 5
    elseif zone.weather == WEATHER.CLOUDY then base = base - 3
    elseif zone.weather == WEATHER.OVERCAST then base = base - 5
    elseif zone.weather == WEATHER.LIGHT_RAIN then base = base - 8
    elseif zone.weather == WEATHER.HEAVY_RAIN then base = base - 12
    elseif zone.weather == WEATHER.STORM then base = base - 15
    elseif zone.weather == WEATHER.FOG then base = base - 5
    elseif zone.weather == WEATHER.SNOW then base = base - 20
    end
    
    return base + math.random(-2, 2)  -- Add randomness
end

function calculate_visibility(zone)
    if zone.weather == WEATHER.CLEAR or zone.weather == WEATHER.PARTLY_CLOUDY then
        return 255  -- Unlimited
    elseif zone.weather == WEATHER.CLOUDY or zone.weather == WEATHER.OVERCAST then
        return 200
    elseif zone.weather == WEATHER.LIGHT_RAIN then
        return 150
    elseif zone.weather == WEATHER.HEAVY_RAIN then
        return 80
    elseif zone.weather == WEATHER.STORM then
        return 40
    elseif zone.weather == WEATHER.FOG then
        return 30
    elseif zone.weather == WEATHER.SNOW then
        return 60
    end
    return 255
end

-- Player tracking
function on_player_entered_zone(zone_id)
    local zone = climate_manager.zones[zone_id]
    if zone then
        zone.player_count = zone.player_count + 1
        zone.has_players = true
    end
end

function on_player_exited_zone(zone_id)
    local zone = climate_manager.zones[zone_id]
    if zone then
        zone.player_count = math.max(0, zone.player_count - 1)
        if zone.player_count == 0 then
            zone.has_players = false
        end
    end
end

-- Commands
function get_weather_report(zone_id)
    local zone = climate_manager.zones[zone_id]
    if not zone then return "Unknown weather" end
    
    local msg = WEATHER_MESSAGES[zone.weather] .. "\n"
    msg = msg .. "Temperature: " .. zone.temperature .. "F\n"
    msg = msg .. "Wind: " .. zone.wind_speed .. " mph\n"
    
    if zone.visibility < 255 then
        msg = msg .. "Visibility: " .. zone.visibility .. " feet\n"
    end
    
    return msg
end

function get_time_string(zone_id)
    local zone = climate_manager.zones[zone_id]
    if not zone then return "Unknown time" end
    
    local hour_str
    if zone.hour == 0 then hour_str = "midnight"
    elseif zone.hour == 12 then hour_str = "noon"
    elseif zone.hour < 12 then hour_str = zone.hour .. " AM"
    else hour_str = (zone.hour - 12) .. " PM"
    end
    
    local season_names = {"Spring", "Summer", "Autumn", "Winter"}
    
    return "It is " .. hour_str .. " on day " .. zone.day .. 
           " of " .. season_names[zone.month + 1] .. ", year " .. zone.year
end

-- Event system helper
function trigger_event(event_name, data)
    -- This will be called by C++ event system
    if _G["on_" .. event_name] then
        _G["on_" .. event_name](data)
    end
end

print("Climate Manager loaded successfully!")
