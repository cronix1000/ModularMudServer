-- Healing Shrine Callback Script
-- Contains only on_use and on_create functions

function on_create(context)
    return {
        success = true,
        actionType = "none"
    }
end

function on_use(context)
    if context.action == "pray" or context.action == "use" then
        return {
            success = true,
            actionType = "heal_player",
            message = "You kneel before the shrine and feel divine energy wash over you. Your wounds begin to close.",
            roomMessage = context.userID .. " kneels in prayer before the healing shrine.",
            targetX = 25,
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