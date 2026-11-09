function MeleeSkill(def)
    return {
        name = def.name,
        type = "combat",
        cooldown = def.cooldown or 2.0, -- Default if missing

        -- The Data needed for the logic
        data = {
            multiplier = def.multiplier or 1.0,
            range = def.range or 0,
            milestones = def.milestones or {},
            synergies = def.synergies or {}
        },
        on_execute = function(self, ctx)
            local mult = self.data.multiplier

            for _, s in ipairs(self.data.synergies) do
                -- TODO: Expose GetMastery to Lua or track mastery differently
                if false then -- placeholder
                    if s.type == "buff_dmg" then mult = mult * s.amount end
                    if s.type == "add_tag" then end -- ctx.tags not available
                end
            end

            return {
                success = true,
                actionType = "attack",
                magnitude = mult,
                damageType = def.damageType or "blunt",
                dataString = def.verb or "hit"
            }
        end
    }
end
