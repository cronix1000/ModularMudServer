-- Calcification alchemy skill execution script
skills = skills or {}

skills["calcify"] = {
    name = "Calcification",
    
    on_execute = function(self, ctx)
        local mastery_bonus = ctx.masteryLevel * 0.1
        local yield = 1.0 + mastery_bonus
        
        send_to_char(ctx.sourceID, "You perform the Rite of Calcification. Materials transform into crystallized salts.")
        
        return {
            success = true,
            actionType = "craft",
            magnitude = yield,
            damageType = "physical"
        }
    end
}