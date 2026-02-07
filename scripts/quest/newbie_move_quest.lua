subscribe("RoomEntered", function(data)
    local player_id = data.entity_id
    local room_id = data.room_id

    if room_id == 2 then
        send_to_char(player_id, "Quest Complete: You found the Hidden Grotto!")
    --   mark_dirty(player_id)
    end
end)