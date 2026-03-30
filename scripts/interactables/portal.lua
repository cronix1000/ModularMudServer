-- Portal Callback Script
-- Contains only on_use and on_create functions

function on_create(context)
    return {
        success = true,
        actionType = "none"
    }
end

function on_use(context)
    if context.action == "enter" or context.action == "use" then
        return {
            success = true,
            actionType = "teleport",
            message = "You step through the mystical portal...",
            roomMessage = context.userID .. " disappears through the portal!",
            targetRoomID = 2,
            targetX = 3,
            targetY = 3
        }
    else
        return {
            success = false,
            actionType = "none",
            message = "You can 'enter' the portal to use it."
        }
    end
end