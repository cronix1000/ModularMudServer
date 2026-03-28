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

skills["stab"] = MeleeSkill({
    name = "Quick Stab",
    damage = 8,
    cooldown = 1.5,
    damageType = "piercing",
    
    synergies = {
        -- If player has Lvl 40 Slash, Stab does 1.2x damage
        { skill = "slash", level = 40, type = "buff_dmg", amount = 1.2 },
        -- If player has Lvl 20 Bash, add "bleed" tag
        { skill = "bash", level = 20, type = "add_tag", tag = "bleed" }
    }
})

skills["poke"] = MeleeSkill({
    name = "Precise Poke",
    damage = 10,
    cooldown = 0.2,
    damageType = "piercing",
     
    synergies = {
        -- If player has Lvl 30 Slash, Poke does 1.3x damage
        { skill = "slash", level = 30, type = "buff_dmg", amount = 1.3 },
        -- If player has Lvl 20 Stab, add "poison" tag
        { skill = "stab", level = 20, type = "add_tag", tag = "poison" }
    }
})