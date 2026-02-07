-- Magic Lever Interactable Script
-- This script handles lever interactions for triggering events

-- Template data for this interactable type
local magic_lever = {
    name = "Ancient Lever",
    char = "L",
    color = "&g",
    description = "An old stone lever covered in mysterious runes. You can 'pull' it.",
    type = "lever",
    components = {
        state = "up",
        event_trigger = "door_open",
        target_room = -1
    }
}

function on_create(context)
    print("Magic lever created at room " .. context.roomID)
    return {
        success = true,
        actionType = "none"
    }
end

function on_use(context)
    print("Player " .. context.userID .. " is attempting to use lever " .. context.interactableID)
    
    if context.action == "pull" or context.action == "use" then
        -- Toggle lever state and trigger an event
        return {
            success = true,
            actionType = "trigger_event",
            message = "You pull the ancient lever. You hear a distant rumbling...",
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

-- Return the template data
return magic_lever