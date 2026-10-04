#pragma once
#include "MetaComponent.h"
#include <nlohmann/json.hpp>
#include <string>

namespace MetaRegistry {

enum class KeyType { Int, Float, Bool, String };

struct KeySpec {
	const char* key;
	KeyType type;
	double defaultDouble;
	std::string defaultString;
	bool defaultBool;
	double minVal;
	double maxVal;
	int maxLen;
	bool hasMin;
	bool hasMax;
	bool hasMaxLen;
};

struct EntitySpec {
	const char* entityType;
	const KeySpec* keys;
	int keyCount;
};

// Per-entity-type key tables. Mirror of MudAdmin/app/utils/metaKeys.ts.
// Hand-maintained until codegen is added. The C++ side falls back to
// defaults for unknown or invalid values; the JS side validates at save.

const KeySpec MOB_KEYS[] = {
	{ "respawn_seconds",  KeyType::Int,    300, "", false, 0, 86400, 0, true, true, false },
	{ "respawn_variance", KeyType::Int,    60,  "", false, 0, 3600,  0, true, true, false },
	{ "aggro_radius",     KeyType::Int,    0,   "", false, 0, 30,    0, true, true, false },
	{ "xp_reward",        KeyType::Int,    0,   "", false, 0, 100000,0, true, true, false },
	{ "death_line",       KeyType::String, 0,   "", false, 0, 0,     200, false, false, true },
	{ "taunt_on_hit",     KeyType::String, 0,   "", false, 0, 0,     200, false, false, true },
	{ "faction",          KeyType::String, 0,   "", false, 0, 0,     50,  false, false, true },
};

const KeySpec ROOM_KEYS[] = {
	{ "is_safe",      KeyType::Bool,   0, "", false, 0, 0, 0, false, false, false },
	{ "light_level",  KeyType::Int,    0, "", false, 0, 10, 0, true, true, false },
	{ "brawl_chance", KeyType::Float,  0, "", false, 0, 1, 0, true, true, false },
	{ "climate_tag",  KeyType::String, 0, "", false, 0, 0, 30, false, false, true },
	{ "ambient_sound",KeyType::String, 0, "", false, 0, 0, 50, false, false, true },
};

const KeySpec ITEM_KEYS[] = {
	{ "max_stack",       KeyType::Int,  1,    "", false, 1, 999, 0, true, true, false },
	{ "bind_on_pickup",  KeyType::Bool, 0,    "", false, 0, 0, 0, false, false, false },
	{ "bind_on_equip",   KeyType::Bool, 0,    "", false, 0, 0, 0, false, false, false },
	{ "no_sell",         KeyType::Bool, 0,    "", false, 0, 0, 0, false, false, false },
	{ "no_drop",         KeyType::Bool, 0,    "", false, 0, 0, 0, false, false, false },
	{ "durability",      KeyType::Int,  100,  "", false, 0, 1000, 0, true, true, false },
	{ "required_level",  KeyType::Int,  1,    "", false, 1, 100, 0, true, true, false },
	{ "rarity",          KeyType::String,0,   "common", false, 0, 0, 20, false, false, true },
};

const KeySpec INTERACTABLE_KEYS[] = {
	{ "cooldown_seconds",   KeyType::Int, 0,  "", false, 0, 86400, 0, true, true, false },
	{ "max_uses_per_player",KeyType::Int, -1, "", false, -1, 1000, 0, true, true, false },
};

const EntitySpec ENTITY_SPECS[] = {
	{ "mob",         MOB_KEYS,         sizeof(MOB_KEYS)/sizeof(KeySpec) },
	{ "room",        ROOM_KEYS,        sizeof(ROOM_KEYS)/sizeof(KeySpec) },
	{ "item",        ITEM_KEYS,        sizeof(ITEM_KEYS)/sizeof(KeySpec) },
	{ "interactable",INTERACTABLE_KEYS,sizeof(INTERACTABLE_KEYS)/sizeof(KeySpec) },
};

// Validate and apply defaults to a MetaComponent in-place.
// Unknown keys pass through. Invalid values fall back to defaults.
// Returns true if anything was changed.
bool ApplyDefaults(const std::string& entityType, nlohmann::json& meta);

} // namespace MetaRegistry