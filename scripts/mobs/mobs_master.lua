mobs = {}

mobs["goblin"] = {
    name = "Goblin",
    description = "A small, green creature with sharp teeth and a mischievous grin.",
    health = 30,
    attack = 5,
    defense = 2,
    symbol = "g",
    color = "&g",
    stats = {
        strength = 5,
        agility = 3,
        intelligence = 2
    }
}

mobs["sewer_rat"] = {
    name = "Sewer Rat",
    description = "A large, filthy rat that scurries through the sewers.",
    health = 20,
    attack = 3,
    defense = 1,
    symbol = "r",
    color = "&b",
    stats = {
        strength = 2,
        agility = 4,
        intelligence = 1
    }
}

mobs["starting_npc"] = {
    name = "Town Guard",
    description = "A friendly guard who watches over the town square.",
    health = 100,
    attack = 10,
    defense = 5,
    symbol = "n",
    color = "&y",
    ai = "passive",
    stats = {
        strength = 12,
        agility = 8,
        intelligence = 6
    }
}
