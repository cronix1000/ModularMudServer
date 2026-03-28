# Item System Documentation

## Overview

Items are defined in `items.json` and loaded at startup by `ItemFactory`. Item templates are pure data - Lua scripts are only used for proc hooks (on_hit_proc, etc.).

## Item Template Schema

```json
{
  "item_id": {
    "name": "Display Name",
    "description": "Item description text.",
    "char": "/",
    "color": "&y",
    "value": 100,
    "weight": 5,
    "equippable": true,
    "type": "weapon",
    "components": {
      "weapon": { ... },
      "armour": { ... },
      "scripts": { ... }
    }
  }
}
```

## Base Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `name` | string | Yes | Display name shown to players |
| `description` | string | No | Flavor text shown on examine |
| `char` | string | Yes | ASCII character for display |
| `color` | string | Yes | Color code (e.g., `&y` for yellow) |
| `value` | int | Yes | Gold value |
| `weight` | int | Yes | Inventory weight |
| `equippable` | bool | No | Can be equipped (default: false) |
| `type` | string | Yes | Item category: `weapon`, `armour`, `consumable`, `misc` |

## Component Types

### Weapon Component

```json
"components": {
  "weapon": {
    "minDamage": 5,
    "maxDamage": 10,
    "strScaling": 0.8,
    "dexScaling": 0.2,
    "defaultSkill": "slash",
    "damageType": "slashing"
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `minDamage` | int | Minimum damage roll |
| `maxDamage` | int | Maximum damage roll |
| `strScaling` | float | Strength contribution to damage (0.0-1.0) |
| `dexScaling` | float | Dexterity contribution to damage (0.0-1.0) |
| `defaultSkill` | string | Skill ID to use when equipping |
| `damageType` | string | Type: `slashing`, `piercing`, `blunt`, `fire`, `ice`, etc. |

### Armour Component

```json
"components": {
  "armour": {
    "defense": 12,
    "magic_defense": -2,
    "slot": "torso",
    "type": "plate"
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `defense` | int | Physical damage reduction |
| `magic_defense` | int | Magical damage reduction |
| `slot` | string | Equipment slot: `head`, `torso`, `legs`, `feet`, `hands`, `mainhand`, `offhand` |
| `type` | string | Armour type: `plate`, `mail`, `cloth`, `leather` |

### Scripts Component

```json
"components": {
  "scripts": {
    "on_hit_proc": "procs/flame_burst.lua"
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `on_hit_proc` | string | Lua script path to execute on hit |

### Passive Skills Component

```json
"components": {
  "passive_skills": ["skill_mana_regen_minor", "skill_stealth_boost"]
}
```

## Future Enhancements (Phase 5+)

### Inscription System

```json
"inscriptionSlots": 3,
"validInscriptions": ["fire", "ice", "str", "dex", "int"],
"inscriptionRanges": {
  "fire": { "min": 5, "max": 25 },
  "str": { "min": 1, "max": 10 }
}
```

### Set Bonuses

```json
"setId": "knights_set",
"setBonuses": {
  "2": { "str": 5 },
  "4": { "str": 10, "crit_chance": 0.05 }
}
```

### Rarity Tiers

```json
"rarity": "rare",
"rarityTiers": ["common", "uncommon", "rare", "epic", "legendary"],
"namePrefixes": {
  "fire": ["Flaming", "Blazing", "Infernal"],
  "str": ["Mighty", "Brutal"]
}
```

## Item State (SQLite)

When items exist in player inventories, additional data is stored in `player_items.item_state`:

```json
{
  "equipped": false,
  "slot": -1,
  "modifiers": [
    { "type": "str", "value": 5, "source": "crafting" }
  ],
  "inscriptions": [
    { "type": "fire", "value": 15 }
  ],
  "rarity": "uncommon",
  "crafterName": "PlayerName",
  "craftingSeed": 12345
}
```

## Loading Flow

```
Startup
  └── FactoryManager::LoadAllData()
        └── ItemFactory::LoadItemTemplatesFromJSON("items.json")
              └── Parse JSON, create ItemTemplate structs
              └── Store in itemTemplates map

Runtime
  └── ItemFactory::CreateItem(templateID, overrides)
        └── Clone template
        └── Apply JSON overrides
        └── Create ECS entity
        └── Add components (Name, Visual, Weapon/Armour, etc.)
```

## Adding New Items

1. Add entry to `items.json`
2. Follow the schema above
3. Restart server (or use `/reload items` after Phase 4 hotreload)

## Proc Scripts

Proc scripts in `scripts/procs/` receive a context table:

```lua
-- procs/flame_burst.lua
function on_hit_proc(context)
    local target_id = context.target_id
    local damage = context.damage
    
    -- Apply fire damage
    send_to_char(context.source_id, "Your weapon bursts into flames!")
    
    return {
        bonus_damage = 10,
        damage_type = "fire",
        effect_duration = 3
    }
end
```