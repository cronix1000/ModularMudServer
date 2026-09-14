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

## Important Warnings

- No test framework configured - test manually
- Invalid JSON will crash on load (no validation)
- Thread safety: Only main thread accesses Registry
- Use `DestroyTag` + `CleanUpSystem` for entity destruction
- Always check component pointers before use
