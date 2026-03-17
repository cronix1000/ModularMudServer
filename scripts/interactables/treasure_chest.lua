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

