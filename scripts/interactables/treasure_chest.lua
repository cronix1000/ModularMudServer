-- Treasure Chest Interactable Script
-- This script handles treasure chest interactions

-- Template data for this interactable type
interactables["treasure_chest"] = {
    name = "Ornate Treasure Chest",
    char = "C",
    color = "&y",
    description = "A golden chest that gleams in the light. You can 'open' it to see what's inside.",
    type = "chest",
    components = {
        is_locked = false,
        key_id = -1,
        loot_table = "basic_treasure",
        max_uses = 1
    }
}

function on_create(context)
    -- Called when the chest is first created
    print("Treasure chest created at room " .. context.roomID)
    
    return {
        success = true,
        actionType = "none"
    }
end

function on_use(context)
    -- Called when a player opens the chest
    print("Player " .. context.userID .. " is attempting to open chest " .. context.interactableID)
    
    if context.action == "open" or context.action == "use" then
        -- Check if chest has already been opened (this would normally check a state component)
        -- For now, always give treasure
        
        -- Random treasure generation
        local treasures = {"gold_coin", "health_potion", "magic_scroll", "iron_sword"}
        local selected_treasure = treasures[math.random(#treasures)]
        
        return {
            success = true,
            actionType = "spawn_item",
            message = "You open the chest and find a " .. selected_treasure .. "!",
            roomMessage = context.userID .. " opens the treasure chest.",
            spawnItemID = selected_treasure,
            spawnX = context.x,
            spawnY = context.y,
            consumeOnUse = true  -- Remove the chest after use
        }
    else
        return {
            success = false,
            actionType = "none",
            message = "You can 'open' the treasure chest."
        }
    end
end

-- Return the template data
return treasure_chest