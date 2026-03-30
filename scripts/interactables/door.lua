-- Door Callback Script
-- Contains only on_use and on_create functions

function on_create(context)
    return {
        success = true,
        actionType = "none"
    }
end

function on_use(context)
    if context.action == "open" then
        return {
            success = true,
            actionType = "trigger_event",
            message = "You open the door.",
            roomMessage = context.userID .. " opens the door.",
            eventName = "door_opened"
        }
    elseif context.action == "close" then
        return {
            success = true,
            actionType = "trigger_event",
            message = "You close the door.",
            roomMessage = context.userID .. " closes the door.",
            eventName = "door_closed"
        }
    else
        return {
            success = false,
            actionType = "none",
            message = "You can 'open' or 'close' the door."
        }
    end
end