function on_enter_one(playerId, roomId)
    send_to_char(playerId, "Player " .. playerId .. " has entered room " .. roomId)
    -- Additional logic for when a player enters the room can be added here
end