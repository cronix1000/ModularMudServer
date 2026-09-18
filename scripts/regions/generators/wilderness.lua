--[[
    Wilderness generator — open outdoor area. Mostly floor with scattered
    trees, rocks, water. No walls (only room perimeter).

    Input:  config = {
        rooms = number (default 20),
        tree_density = number (default 0.3),
        water_density = number (default 0.1),
        ... (same as cave)
    }

    Requires palette symbols: T (tree), ~ (water), x (rock).
]]

local function randint(lo, hi)
    return math.floor(lo + (hi - lo + 1) * math.random())
end

function generate(config)
    config = config or {}
    local rooms_target = config.rooms or 20
    local width = config.width or 8
    local height = config.height or 8
    local tree_density = config.tree_density or 0.3
    local water_density = config.water_density or 0.1
    local mob_density = config.mob_density or 0.3
    local mob_template = config.mob_template or 'wolf'
    local floor_sym = config.tile_floor or '.'
    local tree_sym = config.tree_sym or 'T'
    local water_sym = config.water_sym or '~'
    local rock_sym = config.rock_sym or 'x'

    math.randomseed(os.time() + math.random(1, 100000))

    local rooms = {}
    local exits = {}
    local spawns = {}

    local dirs = {
        { name = 'north', rev = 'south' },
        { name = 'south', rev = 'north' },
        { name = 'east',  rev = 'west' },
        { name = 'west',  rev = 'east' },
    }

    for i = 1, rooms_target do
        local layout = {}
        for y = 1, height do
            local row = {}
            for x = 1, width do
                if y == 1 or y == height or x == 1 or x == width then
                    row[x] = floor_sym
                else
                    local r = math.random()
                    if r < tree_density then
                        row[x] = tree_sym
                    elseif r < tree_density + water_density then
                        row[x] = water_sym
                    elseif r < tree_density + water_density + 0.05 then
                        row[x] = rock_sym
                    else
                        row[x] = floor_sym
                    end
                end
            end
            layout[y] = table.concat(row)
        end
        local name = (i == 1) and 'Wilderness Edge' or ('Wilderness ' .. tostring(i))
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
