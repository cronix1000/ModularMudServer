-- Fireball skill execution script
skills = skills or {}

skills["fireball"] = {
    name = "Fireball",
    
    on_execute = function(self, ctx)
        local damage = 25 + (ctx.masteryLevel * 5)
        
        send_to_char(ctx.sourceID, "You cast Fireball! You hurl a blazing sphere of fire at your target.")
        
        return {
            success = true,
            actionType = "attack",
            magnitude = damage,
            damageType = "fire",
            addedTags = { "fire", "magic" }
        }
    end
}