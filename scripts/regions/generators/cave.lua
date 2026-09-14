--[[
    Cave generator.

    Input:  config = {
        rooms      = number  -- target number of rooms (default 12)
        width      = number  -- grid width per room (default 6)
        height     = number  -- grid height per room (default 6)
        mob_density = number -- 0..1 chance a room gets a mob spawn (default 0.4)
        mob_template = string -- mob template_id to spawn (default "cave_rat")
        tile_floor = string -- floor symbol (default ".")
        tile_wall  = string -- wall symbol (default "x")
    }

    Output: {
        entry_room_id = number,
        rooms = { {id, name, width, height, layout = {string...}}, ... },
        exits = { {from_room_id, direction, to_room_id}, ... },
        spawns = { {room_id, x, y, type, template_id}, ... },
    }

    This generator produces a tree of rooms. Each room has a 6x6 layout
    where the edges are walls and the inside is floor with random wall
    pillars (for visual interest). Exits go in random directions from each
    room to a randomly-chosen previously-generated room, producing a tree
    instead of a fully connected mesh.
]]

local function randint(lo, hi)
    return math.floor(lo + (hi - lo + 1) * math.random())
end

local function make_layout(width, height, floor_sym, wall_sym)
    local grid = {}
    for y = 1, height do
        local row = {}
        for x = 1, width do
            row[x] = floor_sym
        end
        grid[y] = table.concat(row)
    end
    for y = 1, height do
        local row = {}
        for x = 1, width do
            if y == 1 or y == height or x == 1 or x == width then
                row[x] = wall_sym
            else
                row[x] = floor_sym
            end
        end
        grid[y] = table.concat(row)
    end
    -- Add a couple of decorative pillars
    for _ = 1, 2 do
        local px = randint(2, width - 1)
        local py = randint(2, height - 1)
        local row = grid[py]
        grid[py] = row:sub(1, px - 1) .. wall_sym .. row:sub(px + 1)
    end
    return grid
end

function generate(config)
    config = config or {}
    local rooms_target = config.rooms or 12
    local width = config.width or 6
    local height = config.height or 6
    local mob_density = config.mob_density or 0.4
    local mob_template = config.mob_template or 'cave_rat'
    local floor_sym = config.tile_floor or '.'
    local wall_sym = config.tile_wall or 'x'

    math.randomseed(os.time() + math.random(1, 100000))

    local rooms = {}
    local exits = {}
    local spawns = {}

    local dirs = {
        { name = 'north', dx = 0, dy = -1 },
        { name = 'south', dx = 0, dy = 1 },
        { name = 'east',  dx = 1, dy = 0 },
        { name = 'west',  dx = -1, dy = 0 },
    }

    -- Pick a parent for each new room
    for i = 1, rooms_target do
        local layout = make_layout(width, height, floor_sym, wall_sym)
        local name = (i == 1) and 'Cave Entrance' or ('Cave Chamber ' .. tostring(i))
        rooms[i] = {
            id = i,
            name = name,
            width = width,
            height = height,
            layout = layout,
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
            -- Reverse direction
            local reverse = { north = 'south', south = 'north', east = 'west', west = 'east' }
            table.insert(exits, {
                from_room_id = parent,
                direction = reverse[dir.name],
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
