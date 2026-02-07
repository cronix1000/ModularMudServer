skills["slash"] = MeleeSkill({
    name = "Wide Slash",
    damage = 12,
    cooldown = 2.5,
    damageType = "slashing",
    
    synergies = {
        -- If player has Lvl 50 Poke, Slash does 1.5x damage
        { skill = "poke", level = 50, type = "buff_dmg", amount = 1.5 },
        -- If player has Lvl 30 Bash, add "stun" tag
        { skill = "bash", level = 30, type = "add_tag", tag = "stun" }
    }
})