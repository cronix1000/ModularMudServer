abilities = {}

abilities["stick_poke"] = {
    mana_cost = 0,
    cooldown = 0.5,

    on_cast = function(caster_id, target_id)
        send_to_char(caster_id, "You poke " .. target_id .. " with a stick.")
        send_to_char(target_id, caster_id .. " pokes you with a stick.")
    end,

    on_hit = function(caster_id, target_id)
        -- No additional effect on hit
    end
}