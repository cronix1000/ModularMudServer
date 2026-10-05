# AGENTS.md - Coding Agent Instructions

## Build Commands

### Building
```bash
# Visual Studio (Windows) - Open solution and build
ModularMudServer.sln

# MSBuild command line
msbuild ModularMudServer.sln /p:Configuration=Release
msbuild ModularMudServer.sln /p:Configuration=Debug

# CMake (recommended)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="$(pwd)/vcpkg_installed/x64-windows"
cmake --build build --config Release
```

### Dependencies (vcpkg)
```bash
vcpkg install nlohmann-json sqlite3 sol2 lua
```

### Smoke Test (DB loaders)
```bash
cmake --build build --target test_db_loaders --config Release
./build/Release/test_db_loaders.exe ./mud.db
```
Exercises `SQLiteDatabase::Load*` methods against the live `mud.db`.

## Data Loading (DB-backed)

World content (items, mobs, interactables, skills, dialogues, terrain, regions, rooms, exits, spawns, loot tables) is loaded from the SQLite database `mud.db` tables `world_*` at startup. JSON files in this directory (`items.json`, `mobs.json`, etc.) are no longer read by the running server; they are kept on disk as legacy fallback only.

The flow:
1. `FactoryManager::LoadAllData()` (`FactoryManager.h:30`) calls `ctx.db->Load*()` for each content type.
2. `SQLiteDatabase` queries the `world_*` tables and assembles a `nlohmann::json` object shaped identically to the legacy JSON file the corresponding factory used to consume.
3. Each factory exposes a `LoadXxxFromJson(const json&)` method. The original file-reading path is preserved as `LoadXxxFromJSON(filename)` but is unused at runtime.

When adding a new world_* table or column:
- Add the column to the corresponding `Load*` method in `SQLiteDatabase.cpp` and pass it through `RowToJson()`.
- Map `*_json` columns: object-typed values are merged into the parent JSON; array-typed values get the `_json` suffix stripped.
- Component-style columns (e.g. `world_items.components_json`) need explicit wrapping under `components` in the loader — see `LoadItems()` for the pattern.

### No Test Framework
**Note**: This project currently has no unit tests beyond the DB-loader smoke test. The gitignore references test frameworks but none are configured. To add tests, use Catch2 or GoogleTest.

## Project Overview

A C++17 Entity-Component-System (ECS) MUD (Multi-User Dungeon) server with:
- Hybrid networking (Telnet + WebSocket)
- Lua scripting integration
- SQLite persistence
- JSON data-driven content

## Code Style Guidelines

### Naming Conventions
- **Classes/Structs**: PascalCase (e.g., `CombatSystem`, `PositionComponent`)
- **Methods/Functions**: PascalCase for systems (`ProcessAttack`), camelCase for accessors (`getValue`)
- **Variables**: camelCase (e.g., `targetStats`, `finalDamage`)
- **Member Variables**: No consistent prefix (some use none, some use `m_` - prefer no prefix)
- **Constants**: UPPER_SNAKE_CASE (e.g., `DEFAULT_BUFLEN`)
- **Template Parameters**: PascalCase
- **Files**: PascalCase matching class names (e.g., `CombatSystem.cpp`)

### File Organization
- Headers: `.h` extension
- Implementation: `.cpp` extension
- One class per file (generally)
- Header guards: Use `#pragma once` (preferred over include guards)

### Includes
```cpp
// 1. Header corresponding to this cpp file (for .cpp files)
#include "CombatSystem.h"

// 2. Project headers (alphabetical)
#include "ClientComponent.h"
#include "EventBus.h"
#include "Registry.h"

// 3. Third-party libraries
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

// 4. Standard library
#include <algorithm>
#include <iostream>
#include <vector>

// 5. Using declarations at top of .cpp only
using json = nlohmann::json;
```

### Header Guidelines
- Use forward declarations when possible
- Minimize includes in headers
- Group related forward declarations together
- Mark system pointers as nullable in comments

```cpp
// Good - minimal includes, forward declarations
#pragma once
#include <iostream>

// Forward declarations
class Registry;
class CombatSystem;
struct GameContext;
```

### Formatting
- **Indentation**: Tabs (observed in existing code)
- **Braces**: Same line for functions and classes
- **Line Length**: ~120 characters soft limit
- **Pointers**: `Type* ptr` (asterisk with type, not variable)
- **References**: `Type& ref` (ampersand with type)
- **Templates**: `template<typename T>` (space after template)

```cpp
// Good
void ProcessAttack(int sourceID, int targetID, float damage) {
    auto* targetStats = ctx.registry->GetComponent<StatComponent>(targetID);
    if (!targetStats) return;
}
```

### Error Handling
- Use early returns for guard clauses
- Check pointers before dereferencing
- Use `nullptr` not `NULL`
- Return `bool` for success/failure when appropriate
- No exceptions (use return codes and nullptr checks)

```cpp
// Good pattern
auto* component = registry->GetComponent<StatComponent>(entity);
if (!component) return;

// Guard clause pattern
if (busy && busy->timeLeft > 0) {
    return;
}
```

### ECS Patterns
- Components are plain structs with public data
- Systems contain logic, iterate over components
- Use `registry->view<T>()` for iteration
- Use `registry->GetComponent<T>()` for access (returns pointer)
- Always check component pointers before use

```cpp
// Component (struct with public data)
struct PositionComponent {
    int x, y;
    int roomId;
};

// System iteration pattern
for (EntityID entity : registry->view<CombatIntentComponent>()) {
    auto* intent = registry->GetComponent<CombatIntentComponent>(entity);
    if (!intent) continue;
    // Process...
}
```

### Comments
- Use `//` for single-line comments
- Use `/* */` for multi-line comments
- Document complex algorithms
- Explain "why" not "what"

### Modern C++ Features (C++17)
- Use `auto` for type deduction
- Use `std::unique_ptr` for ownership
- Use `std::vector`, `std::unordered_map`
- Use structured bindings where appropriate
- Use `std::optional` for nullable values

### Database/SQL
- Use prepared statements to prevent SQL injection
- Passwords hashed with SHA-256 + salt
- Use `SQLiteDatabase` wrapper class

### Lua Scripting
- Scripts located in `scripts/` directory
- Use `ScriptManager` for Lua operations
- Expose C++ functions via sol2

### JSON
- Use `nlohmann/json` library
- Parse with `json::parse()`
- Serialize with `.dump()`

## Architecture Notes

- **Registry**: Central ECS manager
- **GameContext**: Dependency container passed to systems
- **Systems**: Processed in specific order in `GameEngine::Update()`
- **EventBus**: Type-safe pub/sub for cross-system communication
- **Factories**: Create entities from JSON templates

## Common Tasks

### Adding a Component
1. Create `MyComponent.h` with struct definition
2. Add `#include "MyComponent.h"` to `Component.h`

### Adding a System
1. Create `MySystem.h/cpp`
2. Add member to `GameEngine.h`
3. Initialize in `GameEngine` constructor
4. Call in `GameEngine::Update()`
5. Clean up in destructor

### Adding an Event
1. Add to `EventType` enum
2. Define event data struct
3. Add to `EventContext` variant
4. Publish with `eventBus->Publish()`

## Lua scripts — the fast iteration lane

### Discovery & reload

- All `*.lua` files under `ModularMudServer/scripts/` are loaded at boot
  (`ScriptManager::load_all_scripts`).
- **Interactable scripts** (`on_use`) hot-reload on every invocation —
  edit the file, next interaction picks it up. No restart.
- **All other scripts** require a server restart.
- Path convention: `scripts/{interactables,skills,mobs,quest,room,systems,regions/generators}/<name>.lua`.

### Hook surface (currently wired)

#### Per-entity hooks (via `ScriptComponent.scripts_path`)

| Trigger key   | Entity type   | Fires from               | Lua signature                      |
|---------------|---------------|--------------------------|------------------------------------|
| `on_enter`    | room          | `MovementSystem`         | `(playerId, roomId)`               |
| `on_exit`     | room          | **NOT WIRED**            | (planned)                          |
| `pulse`       | room          | **NOT WIRED**            | (planned)                          |
| `on_use`      | interactable  | `InteractionSystem`      | `(context)` — returns action table |
| `on_create`   | interactable  | **NOT WIRED**            | (planned)                          |
| `on_use`      | skill         | `SkillSystem`            | `on_execute(self, ctx)`            |
| `on_attack`   | mob           | `CombatStateSystem`      | `(mob_id, target_id)` — resolved at spawn from `mobs[template_id].on_attack` |

#### Global hooks (via `ScriptManager::execute_hook`)

| Hook                  | Fires from              | Signature                        |
|-----------------------|-------------------------|----------------------------------|
| `on_hour_changed`     | `WorldClimateSystem`    | `(zoneId, gameHour)`             |
| `on_weather_changed`  | `WorldClimateSystem`    | `(zoneId, oldW, newW)`           |
| `on_season_changed`   | `WorldClimateSystem`    | `(zoneId, newSeason)`            |

#### EventBus bridges (via `ScriptEventBridge`)

| Event name   | Payload                                            | Source |
|--------------|---------------------------------------------------|--------|
| `RoomEntered`| `{entity_id, room_id}`                            | `MovementSystem` |
| `CombatHit`  | `{attacker_id, victim_id}`                        | `CombatSystem` |
| `XpGain`     | `{player_id, amount, source}`                     | `ScriptManager::GrantExperience` |
| `LevelUp`    | `{player_id, new_level}`                          | `ScriptManager::GrantExperience` |
| `QuestAccept`| `{player_id, quest_id}`                           | `ScriptManager::AcceptQuest` |
| `QuestObjectiveProgress` | `{player_id, quest_id, objective_id, count}` | `ScriptManager::ProgressQuest` |
| `QuestComplete` | `{player_id, quest_id}`                        | `ScriptManager::CompleteQuest` |

### The bridge gap (the `meta` problem)

Lua scripts receive trigger context only. They cannot read entity
metadata from the Postgres row today — `entity.meta` doesn't exist.
**Fix in Layer 2**: expose `components_json` parsed value to Lua via
`get_meta(entity_id)`. Until that ships, scripts must hardcode values
or read them via a different binding.

### Hooks that exist but aren't bridged (silent dead code)

If you write a script with these, nothing will fire:

- `World.GiveExperience(playerId, amount)` — referenced in `quiz.lua`
- `World.MessageLog(msg)` — referenced in `quiz.lua`
- `World.SpawnItemAtEntity(template, npcId)` — referenced in `quiz.lua`
- `add_known_recipe(uid, recipe_id)` — referenced in `feats.lua`
- `get_progression(uid)` — referenced in `feats.lua`

The current bindings available from Lua are:

- `World_GrantExperience(player_id, amount[, source])` — increments XP and
  fires `XpGain` (and `LevelUp` if a threshold is crossed). Source is
  any string, e.g. `"craft:healing_potion"` or `"quest:newbie_move"`.
- `World_AcceptQuest(player_id, quest_id)` / `World_ProgressQuest(...)` /
  `World_CompleteQuest(...)` / `World_IsQuestActive(...)` — quest state
  is stored in `player_players.data` (round-trips with `PlayerVariablesComponent.stringVars`).

### Hooks that are entirely absent (must be added by a coder)

For most game features you'll need to add the hook yourself first.
Common gaps by layer:

- **Layer 1 (done)**: `XpGain`, `LevelUp`, `QuestAccept`,
  `QuestObjectiveProgress`, `QuestComplete`, `on_attack` mob hook.
- **Layer 2**: `on_player_death`, `on_mob_death`, `on_kill`, `on_login`,
  `on_logout`, `on_pickup`, `on_drop`, `on_equip`, `on_unequip`.
- **Layer 3**: `on_say`, `on_emote`, `on_chat` (needs chat bridge),
  `on_buy`, `on_sell` (needs shop system).

### Adding a new hook (the 5-step)

1. Add the `EventType` enum value in `EventBus.h`.
2. Define the event data struct and add it to the `EventContext` variant.
3. Add a `Publish` call from the relevant system (`*.cpp`).
4. Add the bridge in `ScriptEventBridge.h` if you want Lua to see it.
5. (If needed) Add a Lua binding in `ScriptManager::init` using
   `lua.set_function("World_X", ...)` and a corresponding method.

### Recipe runtime

Recipes are loaded from `world_recipes` at startup into
`FactoryManager.recipes`. Each recipe defines inputs, outputs, skill id,
station type, XP gain, and craft time. When a skill returns
`actionType = "craft"` from Lua, `SkillSystem` produces a
`CombatIntentComponent{actionType="craft", skillID, magnitude}` which
`CombatSystem::ProcessCraft` consumes: it looks up the recipe by skill,
validates inputs in the player's inventory, consumes them, spawns the
outputs (also into the inventory), and grants XP via
`ScriptManager::GrantExperience` if `experience_gain > 0`.

### Hot-reload caveat

The `subskills.lua` table is cached. Editing a skill script in
`scripts/skills/` requires a server restart for it to take effect.
Editing an interactable script does not.

### ECS vs data

The C++ side is an ECS. Components are how you ship new behavior.
Components are not how you ship new entity kinds — those go in Postgres.

## Layer 3 — new tables and runtimes

### Room light + one-way exits

- `world_rooms.light INT DEFAULT 0` (0 = dark, 10 = bright). Loaded by
  `PostgresDatabase::LoadRoomJson` into `outRoom["light"]`.
- `world_room_exits.is_one_way BOOLEAN` is now parsed into
  `RoomExit::isOneWay`. The admin's `+ Add exit` flow auto-creates a
  return exit in the opposite direction on the destination room when the
  new exit is not one-way, landing at the source room's spawn point
  (overridable in the target's Exits tab). The C++ loader itself does
  not auto-create return exits at boot — it reads what the admin wrote.

### Factions

- Tables: `world.world_factions`, `world.world_faction_relations`,
  `players.player_faction_standing`.
- C++ side: `FactionFactory` (in `FactoryManager.factions`).
- Per-player standing lives in the dedicated table (queryable for
  reports). Reads via `FactionFactory::GetStanding`; writes via
  `SetStanding` / `AdjustStanding`.
- Lua bindings: `World_GetFactionStanding(player_id, faction_id)`,
  `World_AdjustFactionStanding(player_id, faction_id, delta)`.
- Events: `EventType::FactionChange` bridged to Lua `FactionChange`
  hook with `{player_id, faction_id, old_standing, new_standing}`.

### Shops / economy

- Tables: `world.world_shop_keeper`, `world.world_shop_inventory`;
  `players.player_players.gold`, `bank_balance`.
- C++ side: `ShopFactory` (in `FactoryManager.shops`). Buys/sells
  use the inventory to compute price (item base value × markup or
  markdown, with `price_override` taking precedence).
- Player gold lives in `players.player_players.gold` (typed column,
  also mirrored to `PlayerVariablesComponent.intVars["gold"]` on
  load/save). Bank uses `bank_balance`.
- Lua bindings: `World_GetGold(player_id[, slot])`,
  `World_AddGold(player_id, amount[, slot])`,
  `World_QuoteBuyPrice`, `World_QuoteSellPrice`,
  `World_ShopBuy`, `World_ShopSell`.
- Shop transactions validate inventory space, gold balance, and
  stock counters. On success the InventoryComponent gets
  `InventoryChangedComponent` for sync.

### Mail / Boards (loaders only)

- Tables: `players.player_mail`, `world.world_board`,
  `players.player_board_post`.
- Loaders: `PostgresDatabase::LoadBoards`, `LoadMailFor(player_id,
  folder)`. Runtime *sending* and *posting* are not yet implemented —
  coder task for a future session.

### Classes / Races

- Tables: `world.world_classes`, `world.world_races`;
  `players.player_players.class_id`, `race_id`, `level`.
- Loaders: `PostgresDatabase::LoadClasses`, `LoadRaces`.
- `PlayerData` gains `gold`, `bankBalance`, `classId`, `raceId`,
  `level` fields, populated by `LoadPlayer`. `PlayerFactory::LoadPlayer`
  hydrates them into `PlayerVariablesComponent` (`intVars["gold"|"bank_balance"|"level"]`,
  `stringVars["class_id"|"race_id"]`). Persist on next save.
- Race/class bonus application at level-up is a future coder task.

## Important Warnings

- No test framework configured - test manually
- Invalid JSON will crash on load (no validation)
- Thread safety: Only main thread accesses Registry
- Use `DestroyTag` + `CleanUpSystem` for entity destruction
- Always check component pointers before use
