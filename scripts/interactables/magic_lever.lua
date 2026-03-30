-- Magic Lever Callback Script
-- Contains only on_use and on_create functions

function on_create(context)
    return {
        success = true,
        actionType = "none"
    }
end

function on_use(context)
    if context.action == "pull" or context.action == "use" then
        return {
            success = true,
            actionType = "trigger_event",
            message = "You pull the lever. You hear a distant rumbling...",
            roomMessage = context.userID .. " pulls the lever with a loud *CLUNK*!",
            eventName = "door_opened",
            eventParams = {"secret_door", tostring(context.roomID)},
            newState = "down"
        }
    else
        return {
            success = false,
            actionType = "none",
            message = "You can 'pull' the lever."
        }
    end
end