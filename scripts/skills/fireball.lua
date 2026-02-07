abilities["fireball"] = {
    mana_cost = 10,
    cooldown = 2.0,

    on_cast = function(caster_id, target_id)
        send_to_char(caster_id, "You cast Fireball at " .. target_id .. "!")
        send_to_char(target_id, "You are hit by a Fireball from " .. caster_id .. "!")
    end
}