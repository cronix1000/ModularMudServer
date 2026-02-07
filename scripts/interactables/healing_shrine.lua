-- Healing Shrine Interactable Script
-- This script handles healing shrine interactions

-- Template data for this interactable type
interactables["healing_shrine"] = {
    name = "Shrine of Healing",
    char = "S",
    color = "&w",
    description = "A divine shrine emanating healing energy. You can 'pray' at it.",
    type = "shrine",
    components = {
        heal_amount = 25,
        cooldown = 300,
        max_uses_per_player = 3
    }
}

function on_create(context)
    print("Healing shrine created at room " .. context.roomID)
    return {
        success = true,
        actionType = "none"
    }
end

function on_use(context)
    print("Player " .. context.userID .. " is praying at shrine " .. context.interactableID)
    
    if context.action == "pray" or context.action == "use" then
        -- Heal the player
        return {
            success = true,
            actionType = "heal_player",
            message = "You kneel before the shrine and feel divine energy wash over you. Your wounds begin to close.",
            roomMessage = context.userID .. " kneels in prayer before the healing shrine.",
            targetX = 25,  -- Using targetX as heal amount for simplicity
            eventName = "player_healed",
            eventParams = {tostring(context.userID), "25"}
        }
    else
        return {
            success = false,
            actionType = "none",
            message = "You can 'pray' at the shrine to receive healing."
        }
    end
end

-- Return the template data
return interactables["healing_shrine"]