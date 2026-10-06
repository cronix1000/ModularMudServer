function on_enter(playerId, roomId)
    send_to_char(playerId, "Player " .. playerId .. " has entered room " .. roomId)
end