RegisterCommand{
    name = "@@c", permission = 0,
    handler = function(client, params)
        local player_id = client.playerEntityID
        return Circle.state.attempt(player_id, params[1])
    end,
}