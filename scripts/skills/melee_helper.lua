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
            send_to_char(ctx.sourceID, "You use " .. self.name .. " dealing " .. dmg .. " damage!") 
            -- Synergy Logic is now native Lua!
            -- Note: GetMastery is not exposed to Lua, using ctx.masteryLevel instead
            for _, s in ipairs(self.data.synergies) do
                -- For now, skip mastery check since GetMastery isn't bound
                -- TODO: Expose GetMastery to Lua or track mastery differently
                if false then -- placeholder
                    if s.type == "buff_dmg" then dmg = dmg * s.amount end
                    if s.type == "add_tag" then end -- ctx.tags not available
                end
            end
            
            return {
                success = true,
                actionType = "attack",
                magnitude = dmg * (1 + ctx.masteryLevel * self.data.scaling / 100),
                damageType = def.damageType or "blunt"
            } 
        end
    }
end