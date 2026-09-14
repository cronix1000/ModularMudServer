--[[
    Dungeon generator — similar to cave but adds locked doors and
    treasure rooms. Player must find keys to progress.

    Input:  config = {
        rooms = number (default 15),
        treasure_room = bool (default true),
        ... (same as cave)
    }

    Output: same shape as cave, with extra "tags" on rooms:
      rooms[i].tags = { "treasure", "locked", "key" }
]]

local function randint(lo, hi)
    return math.floor(lo + (hi - lo + 1) * math.random())
end

function generate(config)
    config = config or {}
    local rooms_target = config.rooms or 15
    local width = config.width or 6
    local height = config.height or 6
    local mob_density = config.mob_density or 0.5
    local mob_template = config.mob_template or 'skeleton_warrior'
    local floor_sym = config.tile_floor or '.'
    local wall_sym = config.tile_wall or 'x'

    math.randomseed(os.time() + math.random(1, 100000))

    local rooms = {}
    local exits = {}
    local spawns = {}

    local dirs = {
        { name = 'north', rev = 'south', dx = 0, dy = -1 },
        { name = 'south', rev = 'north', dx = 0, dy = 1 },
        { name = 'east',  rev = 'west',  dx = 1, dy = 0 },
        { name = 'west',  rev = 'east',  dx = -1, dy = 0 },
    }

    local treasure_idx = randint(2, rooms_target)

    for i = 1, rooms_target do
        local layout = {}
        for y = 1, height do
            local row = ('%s '):rep(width):format(wall_sym):sub(1, width)
            for x = 1, width do
                if y == 1 or y == height or x == 1 or x == width then
                    row = row:sub(1, x - 1) .. wall_sym .. row:sub(x + 1)
                else
                    row = row:sub(1, x - 1) .. floor_sym .. row:sub(x + 1)
                end
            end
            layout[y] = row
        end
        local tags = {}
        local name = (i == 1) and 'Dungeon Entrance' or ('Dungeon Room ' .. tostring(i))
        if i == treasure_idx and config.treasure_room ~= false then
            name = 'Treasure Chamber'
            table.insert(tags, 'treasure')
            table.insert(layout[3], 0) -- no-op marker
        end
        rooms[i] = {
            id = i,
            name = name,
            width = width,
            height = height,
            layout = layout,
            tags = tags,
        }
        if math.random() < mob_density then
            table.insert(spawns, {
                room_id = i,
                x = randint(2, width - 1),
                y = randint(2, height - 1),
                type = 'mob',
                template_id = mob_template,
            })
        end
        if i > 1 then
            local parent = randint(1, i - 1)
            local dir = dirs[randint(1, 4)]
            table.insert(exits, {
                from_room_id = i,
                direction = dir.name,
                to_room_id = parent,
            })
            table.insert(exits, {
                from_room_id = parent,
                direction = dir.rev,
                to_room_id = i,
            })
        end
    end

    return {
        entry_room_id = 1,
        rooms = rooms,
        exits = exits,
        spawns = spawns,
    }
end
