-- Portal Interactable Script
-- This script handles portal interactions for teleportation

-- Template data for this interactable type
interactables["portal"] = {
    name = "Mystical Portal",
    char = "O",
    color = "&c",
    description = "A swirling portal of magical energy. You can 'enter' it to travel.",
    type = "portal",
    components = {
        destination_room = -1,
        direction_command = "enter",
        is_open = true,
        is_locked = false,
        key_id = -1
    }
}

function on_create(context)
    -- Called when the portal is first created
    print("Portal created at room " .. context.roomID .. " at position (" .. context.x .. "," .. context.y .. ")")
    
    return {
        success = true,
        actionType = "none"
    }
end

function on_use(context)
    -- Called when a player uses the portal (enters it)
    print("Player " .. context.userID .. " is attempting to use portal " .. context.interactableID)
    
    -- Note: Component data is now stored in the PortalComponent
    -- The ScriptManager can provide access to component data if needed
    -- For now, we'll use a default destination
    local destination_room = 2  -- This should come from PortalComponent
    local dest_x = 3
    local dest_y = 3
    
    if context.action == "enter" or context.action == "use" then
        return {
            success = true,
            actionType = "teleport",
            message = "You step through the mystical portal...",
            roomMessage = context.userID .. " disappears through the portal!",
            targetRoomID = destination_room,
            targetX = dest_x,
            targetY = dest_y
        }
    else
        return {
            success = false,
            actionType = "none",
            message = "You can 'enter' the portal to use it."
        }
    end
end

-- Return the template data
return interactables["portal"]