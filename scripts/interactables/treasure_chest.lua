-- Treasure Chest Callback Script
-- Contains only on_use and on_create functions

function on_create(context)
    return {
        success = true,
        actionType = "none"
    }
end

function on_use(context)
    if context.action == "open" or context.action == "use" then
        return {
            success = true,
            actionType = "spawn_item",
            message = "You open the chest and find treasures inside!",
            roomMessage = context.userID .. " opens the treasure chest!"
        }
    else
        return {
            success = false,
            actionType = "none",
            message = "You can 'open' the chest."
        }
    end
end