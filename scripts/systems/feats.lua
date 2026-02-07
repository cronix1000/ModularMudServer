local feats_by_trigger = {
    ["combat"] = { 
        "brawler", "sword_master", "pyromancer" 
    },
    ["craft"] = { 
        "apprentice_alchemist", "master_smith" 
    }
}

local feats = {
    ["apprentice_alchemist"] = {
        tracker = "potions_brewed",
        target = 50,
        reward_msg = "You have mastered basic brewing! (Unlocked: Acid Vial)",
        on_unlock = function(pid)
            -- Give the player a new recipe or skill
            add_known_recipe(pid, "acid_vial") 
        end
    }
}

subscribe("on_progress_event", function(data)
    local player_id = data.entity_id
    local feat_key = data.feat_key
    
    local prog = get_progression(pid)

    local feat_key = feats_by_trigger[feat_key]
    if not feat_key then
        return
    end

    

    if feat and progress >= feat.target then
        send_to_char(player_id, feat.reward_msg)
        feat.on_unlock(player_id)
    end
end)