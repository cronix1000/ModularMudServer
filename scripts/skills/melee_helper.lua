function MeleeSkill(def)
    return {
        name = def.name,
        type = "combat",
        cooldown = def.cooldown or 2.0, -- Default if missing
        
        -- The Data needed for the logic
        data = {
            base_damage = def.damage or 5,
            scaling = def.scaling or 1.0,
            range = def.range or 0,
            milestones = def.milestones or {},
            synergies = def.synergies or {}
        },

        -- The Logic (The Helper you asked for)
        -- We inject the 'standard_melee' logic directly here
        on_execute = function(self, ctx)
            -- 'self' refers to this specific skill table (Skills["slash"])
            local dmg = self.data.base_damage
            
            -- Synergy Logic is now native Lua!
            for _, s in ipairs(self.data.synergies) do
                if GetMastery(ctx.source, s.skill) >= s.level then
                    -- Apply synergy
                    if s.type == "buff_dmg" then dmg = dmg * s.amount end
                    if s.type == "add_tag" then table.insert(ctx.tags, s.tag) end
                end
            end
            
            return {
                success = true,
                actionType = "attack",
                magnitude = dmg * (1 + ctx.mastery * self.data.scaling / 100),
                damageType = def.damageType or "blunt"
            } 
        end
    }
end