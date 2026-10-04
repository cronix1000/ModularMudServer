-- newbie_move_quest.lua
-- Tracks whether the player has reached room 2 ("Hidden Grotto").

subscribe("RoomEntered", function(data)
    local player_id = data.entity_id
    local room_id = data.room_id

    if room_id ~= 2 then return end

    if not World_IsQuestActive(player_id, "newbie_move") then
        World_AcceptQuest(player_id, "newbie_move")
        send_to_char(player_id, "Quest Accepted: Reach the Hidden Grotto.")
    end

    if World_IsQuestActive(player_id, "newbie_move") then
        World_ProgressQuest(player_id, "newbie_move", "reach_grotto", 1)
        send_to_char(player_id, "Quest Complete: You found the Hidden Grotto!")
        World_CompleteQuest(player_id, "newbie_move")
    end
end)

subscribe("QuestComplete", function(data)
    if data.quest_id == "newbie_move" then
        send_to_char(data.player_id, "You feel a small surge of experience.")
        World_GrantExperience(data.player_id, 25, "quest:newbie_move")
    end
end)