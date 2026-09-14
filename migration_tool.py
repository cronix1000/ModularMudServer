#!/usr/bin/env python3
"""
migrate_json_to_sql.py
======================

One-shot migration script: reads the current JSON content files for
ModularMudServer and inserts them into the SQLite database (mud.db),
creating the new world-side tables defined by the 2026-08-11 voice memo.

Player tables (`players`, `player_items`) are NOT touched.

Tables created (all idempotent, all keyed by (world_id, ...)):

    world_worlds
    world_regions
    world_rooms
    world_room_exits
    world_room_spawns
    world_terrains
    world_items
    world_mobs
    world_interactables
    world_dialogues
    world_skill_categories
    world_skills
    world_loot_tables
    world_region_overrides
    world_field_definitions   (seeded with a starter set; engine should
                               re-export via `modularmudserver --export-schema`)

Naming convention (2026-08-13):
    player_*   -> engine runtime state (player_players, player_items)
    world_*    -> world content (admin-edited, engine-read)
    unprefixed -> admin infrastructure (users, audit_log) — names are
                  unambiguous, no prefix needed

The engine's existing `players` table is renamed to `player_players` via
ALTER TABLE (preserves data). All `world_*` tables are new names; if the
old un-prefixed names exist from a previous run, they are dropped before
the new ones are created.

Usage
-----

    # default: cwd, looks for ./mud.db, ./regions/, ./*.json
    python3 migrate_json_to_sql.py

    # explicit
    python3 migrate_json_to_sql.py \
        --db ./mud.db \
        --json-dir . \
        --world-id default \
        --world-name "Hell Hath No Room" \
        --reset     # wipe world data before re-inserting (default: True)

Notes
-----
- Pure stdlib. No pip install needed.
- Re-runnable. `--reset` (default) clears all world_id-scoped rows before
  re-inserting so it's safe to run after editing JSON.
- Multi-world ready: every world-scoped table has a `world_id` FK. Only one
  world ("default") is seeded today; pick a different --world-id to add more.
- Engine still reads JSON until you ship `World::LoadWorldFromSQL(...)`.
  This script is the data migration; the engine change is separate.
"""

from __future__ import annotations

import argparse
import json
import sqlite3
import sys
from pathlib import Path
from typing import Any, Iterable


# ---------------------------------------------------------------------------
# Schema
# ---------------------------------------------------------------------------

SCHEMA_SQL = """
-- ---------------------------------------------------------------------------
-- Drop pre-namespace-rename world tables (idempotent: no-op if absent)
-- This handling keeps the migration re-runnable from any prior state.
-- The player table rename (players -> player_players) is handled in
-- Python below because SQLite raises an error if ALTER RENAME targets
-- a missing table.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS regions;
DROP TABLE IF EXISTS rooms;
DROP TABLE IF EXISTS room_exits;
DROP TABLE IF EXISTS room_spawns;
DROP TABLE IF EXISTS terrains;
DROP TABLE IF EXISTS items;
DROP TABLE IF EXISTS mobs;
DROP TABLE IF EXISTS interactables;
DROP TABLE IF EXISTS dialogues;
DROP TABLE IF EXISTS skill_categories;
DROP TABLE IF EXISTS skills;
DROP TABLE IF EXISTS loot_tables;
DROP TABLE IF EXISTS region_overrides;
DROP TABLE IF EXISTS field_definitions;
DROP TABLE IF EXISTS worlds;

-- ---------------------------------------------------------------------------
-- Multi-world container
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS world_worlds (
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL,
    description TEXT,
    created_at  INTEGER DEFAULT (strftime('%s', 'now'))
);

-- One row per region (a region = a "floor" in the engine's current parlance)
CREATE TABLE IF NOT EXISTS world_regions (
    id                  TEXT NOT NULL,
    world_id            TEXT NOT NULL REFERENCES world_worlds(id),
    name                TEXT NOT NULL,
    description         TEXT,
    theme               TEXT,
    floor_settings_json TEXT,                       -- full floor_settings.json blob
    PRIMARY KEY (world_id, id)
);

-- One row per room
CREATE TABLE IF NOT EXISTS world_rooms (
    world_id     TEXT NOT NULL,
    region_id    TEXT NOT NULL,
    room_id      INTEGER NOT NULL,
    name         TEXT NOT NULL,
    description  TEXT,
    terrain      TEXT,
    width        INTEGER,
    height       INTEGER,
    layout_json  TEXT,                              -- 2D grid: list of strings
    spawn_x      INTEGER,
    spawn_y      INTEGER,
    scripts_json TEXT,                              -- e.g. {"on_enter": "..."}
    extra_json   TEXT,
    PRIMARY KEY (world_id, region_id, room_id),
    FOREIGN KEY (world_id, region_id) REFERENCES world_regions(world_id, id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_world_rooms_region ON world_rooms(world_id, region_id);

-- Exits: separate table (per MUD-Worldbuilding learnings) instead of Room fields
CREATE TABLE IF NOT EXISTS world_room_exits (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    world_id     TEXT NOT NULL,
    region_id    TEXT NOT NULL,
    from_room_id INTEGER NOT NULL,
    direction    TEXT NOT NULL,                     -- north|south|east|west|up|down
    to_room_id   INTEGER NOT NULL,
    dest_x       INTEGER DEFAULT -1,
    dest_y       INTEGER DEFAULT -1,
    is_one_way   INTEGER DEFAULT 0,
    is_portal    INTEGER DEFAULT 0,
    portal_name  TEXT,
    auto_trigger INTEGER DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_world_exits_from ON world_room_exits(world_id, region_id, from_room_id);

-- Spawns: resolved from each room's spawn_legend + spawns grid
CREATE TABLE IF NOT EXISTS world_room_spawns (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    world_id      TEXT NOT NULL,
    region_id     TEXT NOT NULL,
    room_id       INTEGER NOT NULL,
    x             INTEGER NOT NULL,
    y             INTEGER NOT NULL,
    type          TEXT NOT NULL,                    -- mob|item|interactable|npc
    template_id   TEXT NOT NULL,
    respawn_time  REAL DEFAULT 30.0,
    is_respawning INTEGER DEFAULT 1,
    override_json TEXT
);

-- Terrains (replaces global_terrain.json)
CREATE TABLE IF NOT EXISTS world_terrains (
    world_id     TEXT NOT NULL,
    symbol       TEXT NOT NULL,
    name         TEXT NOT NULL,
    color        TEXT,
    blocks_move  INTEGER DEFAULT 0,
    blocks_sight INTEGER DEFAULT 0,
    move_cost    INTEGER DEFAULT 1,
    PRIMARY KEY (world_id, symbol)
);

-- Item templates
CREATE TABLE IF NOT EXISTS world_items (
    world_id        TEXT NOT NULL,
    template_id     TEXT NOT NULL,
    name            TEXT NOT NULL,
    description     TEXT,
    char            TEXT,
    color           TEXT,
    value           INTEGER DEFAULT 0,
    weight          INTEGER DEFAULT 0,
    equippable      INTEGER DEFAULT 0,
    type            TEXT,
    components_json TEXT,
    script_ref      TEXT,
    PRIMARY KEY (world_id, template_id)
);

-- Mob/NPC templates (NPCs are mobs with ai='passive'/'npc')
CREATE TABLE IF NOT EXISTS world_mobs (
    world_id            TEXT NOT NULL,
    template_id         TEXT NOT NULL,
    name                TEXT NOT NULL,
    description         TEXT,
    char                TEXT,
    color               TEXT,
    hp                  INTEGER DEFAULT 1,
    level               INTEGER DEFAULT 1,
    ai                  TEXT,
    loot_drop           TEXT,
    strength            INTEGER DEFAULT 0,
    dexterity           INTEGER DEFAULT 0,
    intelligence        INTEGER DEFAULT 0,
    attack_damage       INTEGER DEFAULT 0,
    attack_speed        REAL DEFAULT 0,
    crit_chance         REAL DEFAULT 0,
    crit_mult           REAL DEFAULT 1.5,
    attack_patterns_json TEXT,
    script_ref          TEXT,
    extra_json          TEXT,
    PRIMARY KEY (world_id, template_id)
);

-- Interactable templates
CREATE TABLE IF NOT EXISTS world_interactables (
    world_id        TEXT NOT NULL,
    template_id     TEXT NOT NULL,
    name            TEXT NOT NULL,
    description     TEXT,
    char            TEXT,
    color           TEXT,
    components_json TEXT,
    script_ref      TEXT,
    PRIMARY KEY (world_id, template_id)
);

-- Dialogue nodes (one row per node_id)
CREATE TABLE IF NOT EXISTS world_dialogues (
    world_id      TEXT NOT NULL,
    node_id       TEXT NOT NULL,
    text          TEXT,
    idle_json     TEXT,
    combat_json   TEXT,
    death_json    TEXT,
    options_json  TEXT,                              -- [{"text":, "targetNode":, "luaCallback":}]
    PRIMARY KEY (world_id, node_id)
);

-- Skill categories
CREATE TABLE IF NOT EXISTS world_skill_categories (
    world_id      TEXT NOT NULL,
    category_id   TEXT NOT NULL,
    name          TEXT NOT NULL,
    description   TEXT,
    stats_json    TEXT,
    synergy_bonus REAL DEFAULT 0,
    PRIMARY KEY (world_id, category_id)
);

-- Skills
CREATE TABLE IF NOT EXISTS world_skills (
    world_id     TEXT NOT NULL,
    skill_id     TEXT NOT NULL,
    category_id  TEXT,
    name         TEXT NOT NULL,
    description  TEXT,
    type         TEXT,
    activation   TEXT,
    command      TEXT,
    cooldown     REAL DEFAULT 0,
    windup       REAL DEFAULT 0,
    costs_json   TEXT,
    targeting    TEXT,
    range        INTEGER DEFAULT 0,
    script_ref   TEXT,
    PRIMARY KEY (world_id, skill_id)
);

-- Loot tables
CREATE TABLE IF NOT EXISTS world_loot_tables (
    world_id   TEXT NOT NULL,
    table_id   TEXT NOT NULL,
    name       TEXT,
    entries_json TEXT,
    PRIMARY KEY (world_id, table_id)
);

-- Per-region overrides (floor_settings.json 'overrides' block)
CREATE TABLE IF NOT EXISTS world_region_overrides (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    world_id      TEXT NOT NULL,
    region_id     TEXT NOT NULL,
    target_type   TEXT NOT NULL,
    target_id     TEXT NOT NULL,
    override_json TEXT NOT NULL
);

-- field_definitions: schema-driven forms (the MUD-Admin form renderer reads this)
-- This is a STARTER set; the engine should regenerate via `modularmudserver --export-schema`
CREATE TABLE IF NOT EXISTS world_field_definitions (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    world_id        TEXT NOT NULL,
    entity_type     TEXT NOT NULL,                  -- room|item|mob|interactable|dialogue|skill|loot_table|region
    field_name      TEXT NOT NULL,
    field_type      TEXT NOT NULL,                  -- string|int|real|bool|json|enum
    label           TEXT,
    help_text       TEXT,
    enum_values_json TEXT,
    min_value       REAL,
    max_value       REAL,
    default_value   TEXT,
    editable        INTEGER DEFAULT 1,
    display_order   INTEGER DEFAULT 0,
    region_id       TEXT                            -- non-null => per-region override entry
);

CREATE INDEX IF NOT EXISTS idx_world_field_defs_entity ON world_field_definitions(world_id, entity_type);
"""

# Starter schema for field_definitions (seeded once per world; engine re-exports
# richer entries from C++ components on next run).
FIELD_DEFINITIONS_SEED: list[dict[str, Any]] = [
    # Rooms
    ("room", "name", "string", "Name", "Display name", 1),
    ("room", "description", "string", "Description", "Shown to players on 'look'", 2),
    ("room", "width", "int", "Width", "Grid width", 3),
    ("room", "height", "int", "Height", "Grid height", 4),
    ("room", "terrain", "string", "Terrain", "e.g. city, forest", 5),
    ("room", "spawn_x", "int", "Spawn X", "Default entry x", 6),
    ("room", "spawn_y", "int", "Spawn Y", "Default entry y", 7),
    # Items
    ("item", "name", "string", "Name", "Display name", 1),
    ("item", "description", "string", "Description", "Shown to players on 'look'", 2),
    ("item", "value", "int", "Value", "Gold value", 3),
    ("item", "weight", "int", "Weight", "Carry weight", 4),
    ("item", "type", "enum", "Type", "weapon|armour|consumable|...", 5),
    ("item", "components_json", "json", "Components", "Nested component data", 6),
    # Mobs
    ("mob", "name", "string", "Name", "Display name", 1),
    ("mob", "description", "string", "Description", "Shown to players on 'look'", 2),
    ("mob", "hp", "int", "Max HP", "Hit points", 3),
    ("mob", "level", "int", "Level", "Difficulty tier", 4),
    ("mob", "ai", "enum", "AI", "aggressive|passive|boss|npc", 5),
    ("mob", "strength", "int", "STR", "Strength", 6),
    ("mob", "dexterity", "int", "DEX", "Dexterity", 7),
    ("mob", "intelligence", "int", "INT", "Intelligence", 8),
    ("mob", "attack_damage", "int", "Attack Damage", "Base damage", 9),
    ("mob", "attack_speed", "real", "Attack Speed", "Seconds per attack", 10),
    ("mob", "crit_chance", "real", "Crit Chance", "0.0-1.0", 11),
    ("mob", "crit_mult", "real", "Crit Multiplier", "e.g. 1.5", 12),
    ("mob", "attack_patterns_json", "json", "Attack Patterns", "List of {verb, multiplier, damage_type}", 13),
    ("mob", "loot_drop", "string", "Loot Table", "Loot table ID", 14),
    ("mob", "script_ref", "string", "Script", "Lua script path", 15),
    # Interactables
    ("interactable", "name", "string", "Name", "Display name", 1),
    ("interactable", "description", "string", "Description", "Shown to players", 2),
    ("interactable", "components_json", "json", "Components", "Nested component data", 3),
    ("interactable", "script_ref", "string", "Script", "Lua script path", 4),
    # Dialogues
    ("dialogue", "text", "string", "Text", "Spoken text", 1),
    ("dialogue", "options_json", "json", "Options", "Branching options", 2),
    ("dialogue", "idle_json", "json", "Idle Lines", "Random idle chatter", 3),
    ("dialogue", "combat_json", "json", "Combat Lines", "Combat barks", 4),
    ("dialogue", "death_json", "json", "Death Lines", "Death barks", 5),
    # Skills
    ("skill", "name", "string", "Name", "Display name", 1),
    ("skill", "description", "string", "Description", "What it does", 2),
    ("skill", "type", "enum", "Type", "combat|crafting|utility", 3),
    ("skill", "activation", "enum", "Activation", "weapon|typed", 4),
    ("skill", "command", "string", "Command", "Typed command", 5),
    ("skill", "cooldown", "real", "Cooldown", "Seconds", 6),
    ("skill", "windup", "real", "Windup", "Seconds", 7),
    ("skill", "costs_json", "json", "Costs", "{stamina: X, mana: Y}", 8),
    ("skill", "targeting", "enum", "Targeting", "enemy|ally|none", 9),
    ("skill", "range", "int", "Range", "Reach", 10),
    ("skill", "script_ref", "string", "Script", "Lua script", 11),
    # Loot tables
    ("loot_table", "name", "string", "Name", "Display name", 1),
    ("loot_table", "entries_json", "json", "Entries", "Drop table entries", 2),
    # Regions
    ("region", "name", "string", "Name", "Display name", 1),
    ("region", "description", "string", "Description", "Region flavor text", 2),
    ("region", "theme", "string", "Theme", "e.g. fantasy, sci-fi", 3),
]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _read_json(path: Path, default: Any = None) -> Any:
    if not path.exists():
        return default if default is not None else {}
    try:
        with path.open("r", encoding="utf-8") as f:
            return json.load(f)
    except json.JSONDecodeError as e:
        print(f"  [warn] {path}: invalid JSON - {e}", file=sys.stderr)
        return default if default is not None else {}


def _strip_json(d: dict[str, Any], *keys: str) -> dict[str, Any]:
    """Return a copy of d with the given keys removed."""
    return {k: v for k, v in d.items() if k not in keys}


def _json_or_none(value: Any) -> str | None:
    return json.dumps(value, ensure_ascii=False) if value is not None else None


def _to_int(value: Any, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _to_real(value: Any, default: float = 0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def _to_bool_int(value: Any) -> int:
    return 1 if value else 0


# ---------------------------------------------------------------------------
# Migrators
# ---------------------------------------------------------------------------

def migrate_worlds(conn: sqlite3.Connection, world_id: str, world_name: str, world_desc: str) -> None:
    conn.execute(
        "INSERT OR IGNORE INTO world_worlds (id, name, description) VALUES (?, ?, ?)",
        (world_id, world_name, world_desc),
    )
    print(f"  world_worlds: seeded '{world_id}'")


def reset_world_data(conn: sqlite3.Connection, world_id: str) -> None:
    """Wipe all world_id-scoped rows so the migration is re-runnable."""
    tables = [
        "world_region_overrides", "world_field_definitions", "world_room_spawns",
        "world_room_exits", "world_rooms", "world_regions", "world_terrains",
        "world_items", "world_mobs", "world_interactables", "world_dialogues",
        "world_skill_categories", "world_skills", "world_loot_tables",
    ]
    for t in tables:
        conn.execute(f"DELETE FROM {t} WHERE world_id = ?", (world_id,))


def migrate_terrains(conn: sqlite3.Connection, json_dir: Path, world_id: str) -> None:
    data = _read_json(json_dir / "global_terrain.json", default={})
    count = 0
    for symbol, info in data.items():
        if not isinstance(info, dict) or not symbol:
            continue
        conn.execute(
            """INSERT INTO world_terrains
               (world_id, symbol, name, color, blocks_move, blocks_sight, move_cost)
               VALUES (?, ?, ?, ?, ?, ?, ?)""",
            (
                world_id,
                symbol[0],
                info.get("name", "Unknown"),
                info.get("color"),
                _to_bool_int(info.get("blocks_move", False)),
                _to_bool_int(info.get("blocks_sight", False)),
                _to_int(info.get("move_cost", 1), 1),
            ),
        )
        count += 1
    print(f"  world_terrains: {count} rows")


def migrate_items(conn: sqlite3.Connection, json_dir: Path, world_id: str) -> None:
    data = _read_json(json_dir / "items.json", default={})
    count = 0
    for tid, info in data.items():
        if not isinstance(info, dict):
            continue
        conn.execute(
            """INSERT INTO world_items
               (world_id, template_id, name, description, char, color,
                value, weight, equippable, type, components_json, script_ref)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
            (
                world_id, tid,
                info.get("name", tid),
                info.get("description"),
                info.get("char"),
                info.get("color"),
                _to_int(info.get("value", 0)),
                _to_int(info.get("weight", 0)),
                _to_bool_int(info.get("equippable", False)),
                info.get("type"),
                _json_or_none(info.get("components")),
                info.get("script"),
            ),
        )
        count += 1
    print(f"  world_items: {count} rows")


def migrate_mobs(conn: sqlite3.Connection, json_dir: Path, world_id: str) -> None:
    data = _read_json(json_dir / "mobs.json", default={})
    count = 0
    for tid, info in data.items():
        if not isinstance(info, dict):
            continue
        stats = info.get("stat") or {}
        extra = info.get("extra") or {}
        conn.execute(
            """INSERT INTO world_mobs
               (world_id, template_id, name, description, char, color,
                hp, level, ai, loot_drop,
                strength, dexterity, intelligence,
                attack_damage, attack_speed, crit_chance, crit_mult,
                attack_patterns_json, script_ref, extra_json)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?,
                         ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
            (
                world_id, tid,
                info.get("name", tid),
                info.get("description"),
                info.get("char"),
                info.get("color"),
                _to_int(info.get("hp", 1)),
                _to_int(info.get("level", 1)),
                info.get("ai"),
                info.get("loot_drop") or None,
                _to_int(stats.get("strength", 0)),
                _to_int(stats.get("dexterity", 0)),
                _to_int(stats.get("intelligence", 0)),
                _to_int(info.get("attack_damage", 0)),
                _to_real(info.get("attack_speed", 0)),
                _to_real(info.get("critical_chance", 0)),
                _to_real(info.get("critical_multiplier", 1.5)),
                _json_or_none(info.get("attack_patterns")),
                info.get("script"),
                _json_or_none(extra) if extra else None,
            ),
        )
        count += 1
    print(f"  world_mobs: {count} rows")


def migrate_interactables(conn: sqlite3.Connection, json_dir: Path, world_id: str) -> None:
    data = _read_json(json_dir / "interactables.json", default={})
    count = 0
    for tid, info in data.items():
        if not isinstance(info, dict):
            continue
        conn.execute(
            """INSERT INTO world_interactables
               (world_id, template_id, name, description, char, color, components_json, script_ref)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?)""",
            (
                world_id, tid,
                info.get("name", tid),
                info.get("description"),
                info.get("char"),
                info.get("color"),
                _json_or_none(info.get("components")),
                info.get("script"),
            ),
        )
        count += 1
    print(f"  world_interactables: {count} rows")


def migrate_dialogues(conn: sqlite3.Connection, json_dir: Path, world_id: str) -> None:
    data = _read_json(json_dir / "dialogue.json", default={})
    count = 0
    for node_id, info in data.items():
        if not isinstance(info, dict):
            continue
        # Chatty mob lines (idle/combat/death arrays) vs branching dialogue (text + options)
        is_mob_chatter = "idle" in info or "combat" in info or "death" in info
        if is_mob_chatter:
            conn.execute(
                """INSERT INTO world_dialogues
                   (world_id, node_id, text, idle_json, combat_json, death_json, options_json)
                   VALUES (?, ?, ?, ?, ?, ?, ?)""",
                (
                    world_id, node_id,
                    None,
                    _json_or_none(info.get("idle")),
                    _json_or_none(info.get("combat")),
                    _json_or_none(info.get("death")),
                    None,
                ),
            )
        else:
            conn.execute(
                """INSERT INTO world_dialogues
                   (world_id, node_id, text, idle_json, combat_json, death_json, options_json)
                   VALUES (?, ?, ?, ?, ?, ?, ?)""",
                (
                    world_id, node_id,
                    info.get("text"),
                    None, None, None,
                    _json_or_none(info.get("options")),
                ),
            )
        count += 1
    print(f"  world_dialogues: {count} rows")


def migrate_skills(conn: sqlite3.Connection, json_dir: Path, world_id: str) -> None:
    data = _read_json(json_dir / "skills.json", default={})
    cat_count = 0
    for cid, info in (data.get("skill_categories") or {}).items():
        if not isinstance(info, dict):
            continue
        conn.execute(
            """INSERT INTO world_skill_categories
               (world_id, category_id, name, description, stats_json, synergy_bonus)
               VALUES (?, ?, ?, ?, ?, ?)""",
            (
                world_id, cid,
                info.get("name", cid),
                info.get("description"),
                _json_or_none(info.get("stats")),
                _to_real(info.get("synergyBonus", 0)),
            ),
        )
        cat_count += 1
    skill_count = 0
    for sid, info in (data.get("skills") or {}).items():
        if not isinstance(info, dict):
            continue
        conn.execute(
            """INSERT INTO world_skills
               (world_id, skill_id, category_id, name, description, type,
                activation, command, cooldown, windup, costs_json,
                targeting, range, script_ref)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
            (
                world_id, sid,
                info.get("category"),
                info.get("name", sid),
                info.get("description"),
                info.get("type"),
                info.get("activation"),
                info.get("command"),
                _to_real(info.get("cooldown", 0)),
                _to_real(info.get("windup", 0)),
                _json_or_none(info.get("costs")),
                info.get("targeting"),
                _to_int(info.get("range", 0)),
                info.get("script"),
            ),
        )
        skill_count += 1
    print(f"  world_skill_categories: {cat_count}, world_skills: {skill_count}")


def migrate_loot_tables(conn: sqlite3.Connection, json_dir: Path, world_id: str) -> None:
    data = _read_json(json_dir / "loot_drops.json", default={})
    count = 0
    for tid, entries in data.items():
        if not isinstance(entries, dict):
            continue
        if not entries:
            # skip empty tables (e.g. goblin_loot_table_weak)
            continue
        conn.execute(
            "INSERT INTO world_loot_tables (world_id, table_id, name, entries_json) VALUES (?, ?, ?, ?)",
            (world_id, tid, tid, _json_or_none(entries)),
        )
        count += 1
    print(f"  world_loot_tables: {count} rows (skipped empty)")


def migrate_regions(conn: sqlite3.Connection, json_dir: Path, world_id: str) -> None:
    """Walk regions/<region>/* and migrate each region + its rooms."""
    regions_dir = json_dir / "regions"
    if not regions_dir.is_dir():
        print(f"  regions: no regions/ directory at {regions_dir}, skipping")
        return

    region_count = 0
    room_count = 0
    exit_count = 0
    spawn_count = 0
    override_count = 0

    for region_dir in sorted(regions_dir.iterdir()):
        if not region_dir.is_dir():
            continue
        region_id = region_dir.name
        floor_settings = _read_json(region_dir / "floor_settings.json", default={})

        # Region row
        conn.execute(
            """INSERT INTO world_regions
               (id, world_id, name, description, theme, floor_settings_json)
               VALUES (?, ?, ?, ?, ?, ?)""",
            (
                region_id, world_id,
                region_id,
                None,
                None,
                _json_or_none(floor_settings) if floor_settings else None,
            ),
        )
        region_count += 1

        # Per-region overrides (from floor_settings.overrides.{mobs,items,npcs,interactables,tiles})
        overrides = (floor_settings or {}).get("overrides") or {}
        for target_type in ("mobs", "items", "npcs", "interactables", "tiles"):
            for override in overrides.get(target_type, []):
                target_id = override.get("id")
                if not target_id:
                    continue
                conn.execute(
                    """INSERT INTO world_region_overrides
                       (world_id, region_id, target_type, target_id, override_json)
                       VALUES (?, ?, ?, ?, ?)""",
                    (
                        world_id, region_id,
                        target_type.rstrip("s"),    # normalize to singular
                        target_id,
                        _json_or_none(override),
                    ),
                )
                override_count += 1

        # Rooms (one *.json per room)
        for room_path in sorted(region_dir.glob("*.json")):
            if room_path.name == "floor_settings.json":
                continue
            rData = _read_json(room_path, default={})
            if not rData or "id" not in rData:
                print(f"  [warn] {room_path}: missing id, skipping")
                continue

            room_id = _to_int(rData.get("id"))
            if room_id < 0:
                print(f"  [warn] {room_path}: invalid id={rData.get('id')}, skipping")
                continue

            # Duplicate-id guard: two files in the same region claiming the same room_id
            # is a data bug. Skip with a loud warning so the issue surfaces.
            dup = conn.execute(
                "SELECT 1 FROM world_rooms WHERE world_id=? AND region_id=? AND room_id=?",
                (world_id, region_id, room_id),
            ).fetchone()
            if dup:
                print(f"  [warn] {room_path.name}: duplicate room_id={room_id} in {region_id}; skipping")
                continue

            spawn = rData.get("spawn") or {}
            conn.execute(
                """INSERT INTO world_rooms
                   (world_id, region_id, room_id, name, description, terrain,
                    width, height, layout_json, spawn_x, spawn_y,
                    scripts_json, extra_json)
                   VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                (
                    world_id, region_id, room_id,
                    rData.get("name", f"Room {room_id}"),
                    rData.get("description"),
                    rData.get("terrain"),
                    _to_int(rData.get("width")),
                    _to_int(rData.get("height")),
                    _json_or_none(rData.get("layout")),
                    _to_int(spawn.get("x", -1)) if spawn else -1,
                    _to_int(spawn.get("y", -1)) if spawn else -1,
                    _json_or_none(rData.get("scripts")),
                    _json_or_none(_strip_json(rData, "id", "name", "description",
                                              "width", "height", "layout",
                                              "spawn", "exits", "spawns",
                                              "spawn_legend", "interactables",
                                              "scripts", "terrain")),
                ),
            )
            room_count += 1

            # Exits
            for direction, exit_data in (rData.get("exits") or {}).items():
                if isinstance(exit_data, int):
                    # Legacy shorthand: exits.north = 2
                    to_room, dx, dy = exit_data, -1, -1
                elif isinstance(exit_data, dict):
                    to_room = _to_int(exit_data.get("target_room", -1))
                    dx = _to_int(exit_data.get("dest_x", -1), -1)
                    dy = _to_int(exit_data.get("dest_y", -1), -1)
                else:
                    continue
                if to_room < 0:
                    continue
                conn.execute(
                    """INSERT INTO world_room_exits
                       (world_id, region_id, from_room_id, direction, to_room_id, dest_x, dest_y)
                       VALUES (?, ?, ?, ?, ?, ?, ?)""",
                    (world_id, region_id, room_id, direction, to_room, dx, dy),
                )
                exit_count += 1

            # Spawns (only if both spawns[] and spawn_legend exist)
            spawns = rData.get("spawns")
            legend = rData.get("spawn_legend")
            if isinstance(spawns, list) and isinstance(legend, dict):
                for y, line in enumerate(spawns):
                    if not isinstance(line, str):
                        continue
                    tokens = line.split()
                    for x, symbol in enumerate(tokens):
                        if symbol == "." or symbol not in legend:
                            continue
                        info = legend[symbol]
                        if not isinstance(info, dict):
                            continue
                        spawn_type = info.get("type")
                        template_id = info.get("id")
                        if not spawn_type or not template_id:
                            continue
                        conn.execute(
                            """INSERT INTO world_room_spawns
                               (world_id, region_id, room_id, x, y, type,
                                template_id, respawn_time, is_respawning, override_json)
                               VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                            (
                                world_id, region_id, room_id, x, y,
                                spawn_type, template_id,
                                _to_real(info.get("respawn_time", 30.0)),
                                _to_bool_int(info.get("respawn", True)),
                                _json_or_none(info.get("overrides")),
                            ),
                        )
                        spawn_count += 1

    print(f"  world_regions: {region_count}, world_rooms: {room_count}, world_room_exits: {exit_count}, "
          f"world_room_spawns: {spawn_count}, world_region_overrides: {override_count}")


def migrate_field_definitions(conn: sqlite3.Connection, world_id: str) -> None:
    rows = [
        (world_id, et, fn, ft, label, help_text, None, None, None, None, 1, order, None)
        for (et, fn, ft, label, help_text, order) in FIELD_DEFINITIONS_SEED
    ]
    conn.executemany(
        """INSERT INTO world_field_definitions
           (world_id, entity_type, field_name, field_type, label, help_text,
            enum_values_json, min_value, max_value, default_value,
            editable, display_order, region_id)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
        rows,
    )
    print(f"  world_field_definitions: {len(rows)} seeded rows")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(description="Migrate ModularMudServer JSON content to SQLite")
    parser.add_argument("--db", default="./mud.db", help="Path to SQLite database (default: ./mud.db)")
    parser.add_argument("--json-dir", default=".", help="Directory containing the JSON files (default: cwd)")
    parser.add_argument("--world-id", default="default", help="World ID to seed (default: 'default')")
    parser.add_argument("--world-name", default="Hell Hath No Room", help="World display name")
    parser.add_argument("--world-desc", default="", help="World description")
    parser.add_argument("--no-reset", action="store_true", help="Don't wipe existing world data before inserting")
    args = parser.parse_args()

    db_path = Path(args.db).resolve()
    json_dir = Path(args.json_dir).resolve()

    if not db_path.exists():
        print(f"Error: database not found at {db_path}", file=sys.stderr)
        return 1
    if not json_dir.is_dir():
        print(f"Error: json-dir not a directory: {json_dir}", file=sys.stderr)
        return 1

    print(f"Migrating JSON → SQLite")
    print(f"  db:        {db_path}")
    print(f"  json-dir:  {json_dir}")
    print(f"  world_id:  {args.world_id}")
    print(f"  reset:     {not args.no_reset}")
    print()

    conn = sqlite3.connect(str(db_path))
    conn.execute("PRAGMA foreign_keys = ON")
    try:
        # Conditional player table rename: only if not already renamed.
        # Preserves player data across runs.
        if conn.execute(
            "SELECT 1 FROM sqlite_master WHERE type='table' AND name='players'"
        ).fetchone():
            conn.execute("ALTER TABLE players RENAME TO player_players")
            print("Renamed players -> player_players (preserved data).")

        conn.executescript(SCHEMA_SQL)
        print("Schema OK.")

        if not args.no_reset:
            reset_world_data(conn, args.world_id)
            print(f"Cleared existing rows for world_id='{args.world_id}'.")

        migrate_worlds(conn, args.world_id, args.world_name, args.world_desc)
        migrate_terrains(conn, json_dir, args.world_id)
        migrate_items(conn, json_dir, args.world_id)
        migrate_mobs(conn, json_dir, args.world_id)
        migrate_interactables(conn, json_dir, args.world_id)
        migrate_dialogues(conn, json_dir, args.world_id)
        migrate_skills(conn, json_dir, args.world_id)
        migrate_loot_tables(conn, json_dir, args.world_id)
        migrate_regions(conn, json_dir, args.world_id)
        migrate_field_definitions(conn, args.world_id)

        conn.commit()
        print()
        print("Migration complete.")

        # Summary
        tables = [
            "world_terrains", "world_items", "world_mobs", "world_interactables",
            "world_dialogues", "world_skill_categories", "world_skills",
            "world_loot_tables", "world_regions", "world_rooms", "world_room_exits",
            "world_room_spawns", "world_region_overrides", "world_field_definitions",
        ]
        print()
        print("Row counts:")
        for t in tables:
            cur = conn.execute(f"SELECT COUNT(*) FROM {t} WHERE world_id = ?", (args.world_id,))
            print(f"  {t:26s} {cur.fetchone()[0]}")
        cur = conn.execute("SELECT COUNT(*) FROM player_players")
        print(f"  {'player_players (preserved)':26s} {cur.fetchone()[0]}")
        cur = conn.execute("SELECT COUNT(*) FROM player_items")
        print(f"  {'player_items (preserved)':26s} {cur.fetchone()[0]}")
        cur = conn.execute("SELECT COUNT(*) FROM world_worlds")
        print(f"  {'world_worlds':26s} {cur.fetchone()[0]}")
    finally:
        conn.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())