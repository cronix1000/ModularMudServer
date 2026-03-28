-- Dragon boss Lua AI hook
-- This file is kept because the dragon has complex phase-based combat logic
-- that requires Lua for hot-reload capability

mobs = {}

mobs["dragon_boss"] = {
    name = "Ancient Dragon",
    on_attack = function(mob_id, target_id)
        local roll = math.random()
        
        if roll < 0.3 then
            return {
                verb = "engulfs in flames",
                damage = 80,
                damage_type = "fire",
                is_critical = true
            }
        elseif roll < 0.6 then
            return {
                verb = "swipes with tail",
                damage = 50,
                damage_type = "blunt",
                is_critical = false
            }
        else
            return {
                verb = "claws",
                damage = 40,
                damage_type = "slash",
                is_critical = false
            }
        end
    end
}