#include "PostgresDatabase.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
#include "picosha2.h"
#include "GameContext.h"
#include "Registry.h"
#include "StatComponent.h"
#include "PositionComponent.h"
#include "PlayerComponent.h"
#include "InventoryComponent.h"
#include "EquipmentComponent.h"
#include "ItemComponent.h"
#include "BodyComponent.h"
#include "RegionComponent.h"
#include "PlayerVariablesComponent.h"
#include "TerrainDef.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>

using json = nlohmann::json;

namespace {

bool shouldSkip(const std::string& name, const std::vector<std::string>& skip) {
    for (const auto& s : skip) if (s == name) return true;
    return false;
}

bool isJsonCol(const std::string& name, const std::vector<std::string>& jsonCols) {
    for (const auto& s : jsonCols) if (s == name) return true;
    return false;
}

} // namespace

PostgresDatabase::PostgresDatabase() = default;
PostgresDatabase::~PostgresDatabase() { Disconnect(); }

void PostgresDatabase::LogError(const char* message) {
    std::fprintf(stderr, "[Postgres Error] %s\n", message ? message : "Unknown error");
}

bool PostgresDatabase::Connect(const std::string& connectionString) {
    try {
        conn = std::make_unique<pqxx::connection>(connectionString);
        std::string searchPath;
        try { searchPath = conn->get_var("search_path"); } catch (...) {}
        printf("[Postgres] Connected (search_path=%s)\n", searchPath.c_str());
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] Connect failed: %s\n", e.what());
        conn.reset();
        return false;
    }
}

void PostgresDatabase::Disconnect() {
    if (activeTx) {
        try { activeTx->abort(); } catch (...) {}
        activeTx.reset();
    }
    if (conn) {
        try { conn->close(); } catch (...) {}
        conn.reset();
    }
}

void PostgresDatabase::BeginTransaction() {
    if (!conn) return;
    if (activeTx) {
        std::fprintf(stderr, "[Postgres] BeginTransaction called while another is in flight; aborting prior\n");
        try { activeTx->abort(); } catch (...) {}
        activeTx.reset();
    }
    activeTx = std::make_unique<pqxx::work>(*conn);
}

void PostgresDatabase::EndTransaction() {
    if (!activeTx) return;
    try {
        activeTx->commit();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] EndTransaction commit failed: %s\n", e.what());
        try { activeTx->abort(); } catch (...) {}
    }
    activeTx.reset();
}

// ============================================================================
// RowToJson
// ============================================================================
//
// libpqxx exposes a row as a sequence of pqxx::field objects whose type comes
// from the Postgres result descriptor. We map:
//   numeric      -> int64_t / double
//   boolean      -> bool
//   text/varchar -> std::string
//   jsonb        -> parsed nlohmann::json (same merging rules as SQLite)
//
nlohmann::json PostgresDatabase::RowToJson(const pqxx::row& row,
                                           const std::vector<std::string>& skipColumns,
                                           const std::vector<std::string>& jsonColumns) {
    json out = json::object();
    const auto numCols = row.size();
    for (pqxx::row_size_type i = 0; i < numCols; ++i) {
        const auto& field = row[i];
        const std::string name = field.name();
        if (shouldSkip(name, skipColumns)) continue;
        if (field.is_null()) continue;

        const auto oid = field.type();

        // text / varchar / char / jsonb / unknown-but-string — treat as text
        // and let jsonColumns decide if we should re-parse.
        if (oid == 16) { // bool
            out[name] = field.as<bool>();
            continue;
        }
        if (oid == 20 || oid == 21 || oid == 23) { // int8 / int2 / int4
            out[name] = field.as<long long>();
            continue;
        }
        if (oid == 700 || oid == 701 || oid == 1700) { // float4 / float8 / numeric
            out[name] = field.as<double>();
            continue;
        }
        // Fallback: text-like
        std::string s = field.c_str() ? field.c_str() : "";
        if (isJsonCol(name, jsonColumns) && !s.empty()) {
            try {
                json parsed = json::parse(s);
                if (parsed.is_object()) {
                    for (auto it = parsed.begin(); it != parsed.end(); ++it) {
                        out[it.key()] = it.value();
                    }
                } else if (parsed.is_array()) {
                    const std::string suffix = "_json";
                    std::string baseName = name;
                    if (baseName.size() >= suffix.size() &&
                        baseName.compare(baseName.size() - suffix.size(), suffix.size(), suffix) == 0) {
                        baseName = baseName.substr(0, baseName.size() - suffix.size());
                    }
                    out[baseName] = parsed;
                } else {
                    out[name] = parsed;
                }
            } catch (const std::exception&) {
                out[name] = s;
            }
        } else {
            out[name] = s;
        }
    }
    return out;
}

// ============================================================================
// Player persistence
// ============================================================================

bool PostgresDatabase::SavePlayer(EntityID playerEnt, GameContext& ctx) {
    auto* stats = ctx.registry->GetComponent<StatComponent>(playerEnt);
    auto* pos = ctx.registry->GetComponent<PositionComponent>(playerEnt);
    auto* playerComp = ctx.registry->GetComponent<PlayerComponent>(playerEnt);
    auto* body = ctx.registry->GetComponent<BodyComponent>(playerEnt);
    auto* region = ctx.registry->GetComponent<RegionComponent>(playerEnt);

    if (!stats || !pos || !playerComp) return false;

    json playerData;
    playerData["stats"] = {
        {"hp", stats->Health},
        {"max_hp", stats->MaxHealth},
        {"str", stats->Strength},
        {"dex", stats->Dexterity},
        {"int", stats->Intelligence},
        {"wis", stats->Wisdom},
        {"atk", stats->AttackDamage},
        {"atkspd", stats->attackSpeed},
        {"mana", stats->Mana}
    };

    if (body) {
        playerData["body_mods"] = json::array();
        for (auto const& [slot, mod] : body->activeMutations) {
            playerData["body_mods"].push_back({ {"slot", (int)slot}, {"id", mod.name} });
        }
    }

    auto* vars = ctx.registry->GetComponent<PlayerVariablesComponent>(playerEnt);
    if (vars) {
        playerData["variables"] = {
            {"intVars", vars->intVars},
            {"stringVars", vars->stringVars}
        };
    }

    if (!conn) return false;
    try {
        pqxx::work tx(*conn);
        std::string dataStr = playerData.dump();
        tx.exec_params(
            "UPDATE player_players "
            "SET data = $1::jsonb, room_id = $2, region_id = $3 "
            "WHERE id = $4",
            dataStr,
            pos->roomId,
            region->region,
            playerComp->accountID
        );
        tx.commit();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] SavePlayer failed: %s\n", e.what());
        return false;
    }

    SaveInventory(playerEnt, ctx);
    return true;
}

int PostgresDatabase::CreatePlayerRow(const std::string& name,
                                      const std::string& passwordHash,
                                      const std::string& salt) {
    if (!conn) return -1;
    json defaultData;
    defaultData["stats"] = {
        {"hp", 100}, {"max_hp", 100}, {"str", 10}, {"dex", 10},
        {"int", 10}, {"wis", 10}, {"atk", 5}, {"atkspd", 1.0}, {"mana", 50}
    };
    const std::string dataStr = defaultData.dump();

    try {
        pqxx::work tx(*conn);
        pqxx::result r = tx.exec_params(
            "INSERT INTO player_players "
            "(permission, region_id, name, password_hash, salt, room_id, data) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7::jsonb) "
            "RETURNING id",
            50,
            std::string("floor1"),
            name,
            passwordHash,
            salt,
            1,
            dataStr
        );
        tx.commit();
        if (r.empty()) return -1;
        return r[0][0].as<int>();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] CreatePlayerRow failed: %s\n", e.what());
        return -1;
    }
}

void PostgresDatabase::SaveInventory(EntityID playerEnt, GameContext& ctx) {
    auto playerComp = ctx.registry->GetComponent<PlayerComponent>(playerEnt);
    if (!playerComp || !conn) return;

    auto inv = ctx.registry->GetComponent<InventoryComponent>(playerEnt);
    auto equip = ctx.registry->GetComponent<EquipmentComponent>(playerEnt);

    auto saveItem = [this, &ctx, playerComp](pqxx::work& tx, int itemEnt, bool isEquipped, int slotID) {
        auto item = ctx.registry->GetComponent<ItemComponent>(itemEnt);
        if (!item) return;
        json state;
        state["equipped"] = isEquipped;
        state["slot"] = slotID;
        const std::string stateStr = state.dump();
        tx.exec_params(
            "INSERT INTO player_items (owner_id, template_id, item_state) "
            "VALUES ($1, $2, $3::jsonb)",
            playerComp->accountID,
            item->templateName,
            stateStr
        );
    };

    BeginTransaction();
    if (!activeTx) return;
    try {
        activeTx->exec_params("DELETE FROM player_items WHERE owner_id = $1", playerComp->accountID);
        if (inv) {
            for (int id : inv->items) saveItem(*activeTx, id, false, -1);
        }
        if (equip) {
            for (const auto& slotPair : equip->slots) {
                saveItem(*activeTx, slotPair.second, true, static_cast<int>(slotPair.first));
            }
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] SaveInventory failed: %s\n", e.what());
    }
    EndTransaction();
}

bool PostgresDatabase::LoadPlayer(const std::string& name, PlayerData& outData) {
    if (!conn) return false;
    try {
        pqxx::work tx(*conn);
        pqxx::result r = tx.exec_params(
            "SELECT id, region_id, room_id, permission, data "
            "FROM player_players WHERE name = $1",
            name
        );
        if (r.empty()) return false;

        outData.id = r[0]["id"].as<int>();
        outData.region = r[0]["region_id"].as<std::string>();
        outData.room_id = r[0]["room_id"].as<int>();
        outData.permission = r[0]["permission"].as<int>();
        outData.name = name;
        outData.data = json::parse(r[0]["data"].as<std::string>());

        pqxx::result items = tx.exec_params(
            "SELECT template_id, item_state FROM player_items WHERE owner_id = $1",
            outData.id
        );
        for (const auto& row : items) {
            SavedItemData item;
            item.templateId = row["template_id"].as<std::string>();
            item.state = json::parse(row["item_state"].as<std::string>());
            outData.items.push_back(item);
        }
        tx.commit();
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] LoadPlayer failed: %s\n", e.what());
        return false;
    }
}

bool PostgresDatabase::PlayerExists(const std::string& name) {
    if (!conn) return false;
    try {
        pqxx::work tx(*conn);
        pqxx::result r = tx.exec_params("SELECT 1 FROM player_players WHERE name = $1", name);
        tx.commit();
        return !r.empty();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] PlayerExists failed: %s\n", e.what());
        return false;
    }
}

bool PostgresDatabase::UpdatePassword(const std::string& name,
                                      const std::string& passwordHash,
                                      const std::string& salt) {
    if (!conn) return false;
    try {
        pqxx::work tx(*conn);
        pqxx::result r = tx.exec_params(
            "UPDATE player_players SET password_hash = $1, salt = $2 WHERE name = $3",
            passwordHash, salt, name
        );
        tx.commit();
        return r.affected_rows() > 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] UpdatePassword failed: %s\n", e.what());
        return false;
    }
}

bool PostgresDatabase::VerifyPassword(const std::string& name, const std::string& password) {
    if (!conn) return false;
    try {
        pqxx::work tx(*conn);
        pqxx::result r = tx.exec_params(
            "SELECT password_hash, salt FROM player_players WHERE name = $1",
            name
        );
        tx.commit();
        if (r.empty()) return false;

        const std::string storedHash = r[0]["password_hash"].as<std::string>();
        const std::string salt = r[0]["salt"].as<std::string>();
        const std::string combined = password + salt;
        const std::string computedHash = picosha2::hash256_hex_string(combined);
        return computedHash == storedHash;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] VerifyPassword failed: %s\n", e.what());
        return false;
    }
}

// ============================================================================
// World loading
// ============================================================================

bool PostgresDatabase::LoadTerrain() {
    if (!conn) return false;

    try {
        pqxx::work tx(*conn);
        pqxx::result r = tx.exec(
            "SELECT symbol, name, color, blocks_move, blocks_sight, move_cost "
            "FROM world_terrains"
        );
        int count = 0;
        for (const auto& row : r) {
            const std::string symTxt = row["symbol"].as<std::string>();
            if (symTxt.empty()) continue;
            const char symbol = symTxt[0];
            const std::string name  = row["name"].is_null()  ? "Unknown" : row["name"].as<std::string>();
            const std::string color = row["color"].is_null() ? "white"   : row["color"].as<std::string>();
            const bool blocksMove  = row["blocks_move"].as<bool>();
            const bool blocksSight = row["blocks_sight"].as<bool>();
            const int  moveCost    = row["move_cost"].as<int>();
            globalTerrain[symbol] = TerrainDef(symbol, name, color, blocksMove, blocksSight, moveCost);
            ++count;
        }
        tx.commit();
        printf("[Postgres] Loaded %d terrain symbols from world_terrains.\n", count);
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] LoadTerrain failed: %s\n", e.what());
        return false;
    }
}

nlohmann::json PostgresDatabase::LoadItems(const std::string& worldId) {
    json out = json::object();
    if (!conn) return out;
    try {
        pqxx::work tx(*conn);
        pqxx::result r = tx.exec_params(
            "SELECT template_id, name, description, char, color, value, weight, "
            "       equippable, type, components_json, script_ref "
            "FROM world_items WHERE world_id = $1",
            worldId
        );
        int count = 0;
        for (const auto& row : r) {
            std::string key = row["template_id"].as<std::string>();
            json obj = RowToJson(row, {"template_id"}, {"components_json"});
            if (obj.contains("script_ref")) obj["script"] = obj["script_ref"];
            if (obj.contains("equippable") && obj["equippable"].is_number_integer()) {
                obj["equippable"] = obj["equippable"].get<int>() != 0;
            }
            static const std::vector<std::string> componentKeys = {
                "weapon", "armour", "scripts", "passive_skills", "armor"
            };
            json componentsObj = json::object();
            bool hasComponents = false;
            for (const auto& ck : componentKeys) {
                if (obj.contains(ck)) {
                    componentsObj[ck] = obj[ck];
                    obj.erase(ck);
                    hasComponents = true;
                }
            }
            if (hasComponents) obj["components"] = componentsObj;
            out[key] = obj;
            ++count;
        }
        tx.commit();
        printf("[Postgres] Loaded %d items from world_items.\n", count);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] LoadItems failed: %s\n", e.what());
    }
    return out;
}

nlohmann::json PostgresDatabase::LoadMobs(const std::string& worldId) {
    json out = json::object();
    if (!conn) return out;
    try {
        pqxx::work tx(*conn);
        pqxx::result r = tx.exec_params(
            "SELECT template_id, name, description, char, color, hp, level, ai, loot_drop, "
            "       strength, dexterity, intelligence, attack_damage, attack_speed, crit_chance, "
            "       crit_mult, attack_patterns_json, script_ref, extra_json "
            "FROM world_mobs WHERE world_id = $1",
            worldId
        );
        int count = 0;
        for (const auto& row : r) {
            std::string key = row["template_id"].as<std::string>();
            json obj = RowToJson(row, {"template_id"}, {"attack_patterns_json", "extra_json"});
            if (obj.contains("script_ref")) obj["script"] = obj["script_ref"];
            json stat = json::object();
            if (obj.contains("strength"))     { stat["strength"] = obj["strength"];     obj.erase("strength"); }
            if (obj.contains("dexterity"))    { stat["dexterity"] = obj["dexterity"];   obj.erase("dexterity"); }
            if (obj.contains("intelligence")) { stat["intelligence"] = obj["intelligence"]; obj.erase("intelligence"); }
            obj["stat"] = stat;
            if (obj.contains("crit_chance") && !obj.contains("critical_chance")) {
                obj["critical_chance"] = obj["crit_chance"]; obj.erase("crit_chance");
            }
            if (obj.contains("crit_mult") && !obj.contains("critical_multiplier")) {
                obj["critical_multiplier"] = obj["crit_mult"]; obj.erase("crit_mult");
            }
            out[key] = obj;
            ++count;
        }
        tx.commit();
        printf("[Postgres] Loaded %d mobs from world_mobs.\n", count);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] LoadMobs failed: %s\n", e.what());
    }
    return out;
}

nlohmann::json PostgresDatabase::LoadInteractables(const std::string& worldId) {
    json out = json::object();
    if (!conn) return out;
    try {
        pqxx::work tx(*conn);
        pqxx::result r = tx.exec_params(
            "SELECT template_id, name, description, char, color, components_json, script_ref "
            "FROM world_interactables WHERE world_id = $1",
            worldId
        );
        int count = 0;
        for (const auto& row : r) {
            std::string key = row["template_id"].as<std::string>();
            json obj = RowToJson(row, {"template_id"}, {"components_json"});
            if (obj.contains("script_ref")) obj["script"] = obj["script_ref"];
            static const std::vector<std::string> knownTypes = {
                "portal", "chest", "door", "lever", "healing", "inventory", "loot"
            };
            json componentsObj = json::object();
            for (const auto& k : knownTypes) {
                if (obj.contains(k)) {
                    componentsObj[k] = obj[k];
                    obj.erase(k);
                }
            }
            if (!componentsObj.empty()) obj["components"] = componentsObj;
            out[key] = obj;
            ++count;
        }
        tx.commit();
        printf("[Postgres] Loaded %d interactables from world_interactables.\n", count);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] LoadInteractables failed: %s\n", e.what());
    }
    return out;
}

nlohmann::json PostgresDatabase::LoadSkills(const std::string& worldId) {
    json out = json::object();
    if (!conn) return out;
    try {
        pqxx::work tx(*conn);

        json categories = json::object();
        pqxx::result cats = tx.exec_params(
            "SELECT category_id, name, description, stats_json, synergy_bonus "
            "FROM world_skill_categories WHERE world_id = $1",
            worldId
        );
        int catCount = 0;
        for (const auto& row : cats) {
            std::string key = row["category_id"].as<std::string>();
            json obj = RowToJson(row, {"category_id"}, {"stats_json"});
            if (obj.contains("synergy_bonus") && !obj.contains("synergyBonus")) {
                obj["synergyBonus"] = obj["synergy_bonus"]; obj.erase("synergy_bonus");
            }
            categories[key] = obj;
            ++catCount;
        }

        json skills = json::object();
        pqxx::result sks = tx.exec_params(
            "SELECT skill_id, category_id, name, description, type, activation, command, "
            "       cooldown, windup, costs_json, targeting, range, script_ref "
            "FROM world_skills WHERE world_id = $1",
            worldId
        );
        int skillCount = 0;
        for (const auto& row : sks) {
            std::string key = row["skill_id"].as<std::string>();
            json obj = RowToJson(row, {"skill_id"}, {"costs_json"});
            if (obj.contains("script_ref")) obj["script"] = obj["script_ref"];
            skills[key] = obj;
            ++skillCount;
        }

        tx.commit();
        out["skill_categories"] = categories;
        out["skills"] = skills;
        printf("[Postgres] Loaded %d skill categories and %d skills from world_skills.\n",
               catCount, skillCount);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] LoadSkills failed: %s\n", e.what());
    }
    return out;
}

nlohmann::json PostgresDatabase::LoadLootTables(const std::string& worldId) {
    json out = json::object();
    if (!conn) return out;
    try {
        pqxx::work tx(*conn);
        pqxx::result r = tx.exec_params(
            "SELECT table_id, name, entries_json "
            "FROM world_loot_tables WHERE world_id = $1",
            worldId
        );
        int count = 0;
        for (const auto& row : r) {
            std::string key = row["table_id"].as<std::string>();
            json obj = RowToJson(row, {"table_id"}, {"entries_json"});
            out[key] = obj.contains("entries") ? obj["entries"] : json::array();
            ++count;
        }
        tx.commit();
        printf("[Postgres] Loaded %d loot tables from world_loot_tables.\n", count);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] LoadLootTables failed: %s\n", e.what());
    }
    return out;
}

nlohmann::json PostgresDatabase::LoadDialogues(const std::string& worldId) {
    json out = json::object();
    if (!conn) return out;
    try {
        pqxx::work tx(*conn);
        pqxx::result r = tx.exec_params(
            "SELECT node_id, text, idle_json, combat_json, death_json, options_json "
            "FROM world_dialogues WHERE world_id = $1",
            worldId
        );
        int count = 0;
        for (const auto& row : r) {
            std::string key = row["node_id"].as<std::string>();
            json obj = RowToJson(row, {"node_id"}, {"idle_json", "combat_json", "death_json", "options_json"});
            if (obj.contains("text") && !obj["text"].is_null()) {
                json node = json::object();
                node["text"] = obj["text"];
                if (obj.contains("options") && obj["options"].is_array()) {
                    node["options"] = obj["options"];
                } else {
                    node["options"] = json::array();
                }
                out[key] = node;
            } else {
                json vs = json::object();
                if (obj.contains("idle"))   vs["idle"]   = obj["idle"];
                if (obj.contains("combat")) vs["combat"] = obj["combat"];
                if (obj.contains("death"))  vs["death"]  = obj["death"];
                out[key] = vs;
            }
            ++count;
        }
        tx.commit();
        printf("[Postgres] Loaded %d dialogue entries from world_dialogues.\n", count);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] LoadDialogues failed: %s\n", e.what());
    }
    return out;
}

// ============================================================================
// Region / room loading
// ============================================================================

bool PostgresDatabase::RegionExists(const std::string& worldId, const std::string& regionId) {
    if (!conn) return false;
    try {
        pqxx::work tx(*conn);
        pqxx::result r = tx.exec_params(
            "SELECT 1 FROM world_regions WHERE world_id = $1 AND id = $2 LIMIT 1",
            worldId, regionId
        );
        tx.commit();
        return !r.empty();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] RegionExists failed: %s\n", e.what());
        return false;
    }
}

bool PostgresDatabase::LoadRegionFloorSettings(const std::string& worldId,
                                               const std::string& regionId,
                                               json& outSettings) {
    outSettings = json::object();
    if (!conn) return false;
    try {
        pqxx::work tx(*conn);
        pqxx::result r = tx.exec_params(
            "SELECT floor_settings_json FROM world_regions WHERE world_id = $1 AND id = $2",
            worldId, regionId
        );
        if (!r.empty() && !r[0]["floor_settings_json"].is_null()) {
            const std::string raw = r[0]["floor_settings_json"].as<std::string>();
            if (!raw.empty()) {
                try {
                    outSettings = json::parse(raw);
                } catch (const std::exception& e) {
                    std::cerr << "[Postgres] floor_settings_json parse error for region "
                              << regionId << ": " << e.what() << std::endl;
                    return false;
                }
            }
        }
        tx.commit();
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] LoadRegionFloorSettings failed: %s\n", e.what());
        return false;
    }
}

std::vector<int> PostgresDatabase::LoadRoomIds(const std::string& worldId, const std::string& regionId) {
    std::vector<int> ids;
    if (!conn) return ids;
    try {
        pqxx::work tx(*conn);
        pqxx::result r = tx.exec_params(
            "SELECT room_id FROM world_rooms "
            "WHERE world_id = $1 AND region_id = $2 ORDER BY room_id",
            worldId, regionId
        );
        for (const auto& row : r) ids.push_back(row["room_id"].as<int>());
        tx.commit();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] LoadRoomIds failed: %s\n", e.what());
    }
    return ids;
}

bool PostgresDatabase::LoadRoomJson(const std::string& worldId,
                                    const std::string& regionId,
                                    int roomId,
                                    json& outRoom) {
    outRoom = json::object();
    if (!conn) return false;
    try {
        pqxx::work tx(*conn);
        pqxx::result rs = tx.exec_params(
            "SELECT room_id, name, description, terrain, width, height, layout_json, "
            "       spawn_x, spawn_y, scripts_json, extra_json "
            "FROM world_rooms WHERE world_id = $1 AND region_id = $2 AND room_id = $3",
            worldId, regionId, roomId
        );
        if (rs.empty()) return false;
        outRoom = RowToJson(rs[0], {"world_id", "region_id", "room_id"},
                            {"layout_json", "scripts_json", "extra_json"});
        outRoom["id"] = roomId;

        json exits = json::object();
        pqxx::result er = tx.exec_params(
            "SELECT direction, to_room_id, dest_x, dest_y, is_one_way, is_portal, "
            "       portal_name, auto_trigger "
            "FROM world_room_exits WHERE world_id = $1 AND region_id = $2 AND from_room_id = $3",
            worldId, regionId, roomId
        );
        for (const auto& row : er) {
            const std::string dir = row["direction"].as<std::string>();
            json exitObj = json::object();
            exitObj["target_room"] = row["to_room_id"].as<int>();
            exitObj["dest_x"] = row["dest_x"].as<int>();
            exitObj["dest_y"] = row["dest_y"].as<int>();
            exitObj["is_portal"] = row["is_portal"].as<bool>();
            if (!row["portal_name"].is_null()) {
                exitObj["portal_name"] = row["portal_name"].as<std::string>();
            }
            exitObj["auto_trigger"] = row["auto_trigger"].as<bool>();
            exits[dir] = exitObj;
        }
        if (!exits.empty()) outRoom["exits"] = exits;

        struct SpawnRec { int x; int y; std::string type; std::string tmpl; json ov; float respawnTime; bool respawning; };
        std::vector<SpawnRec> spawns;
        pqxx::result sr = tx.exec_params(
            "SELECT x, y, type, template_id, override_json, respawn_time, is_respawning "
            "FROM world_room_spawns WHERE world_id = $1 AND region_id = $2 AND room_id = $3",
            worldId, regionId, roomId
        );
        for (const auto& row : sr) {
            SpawnRec rec;
            rec.x = row["x"].as<int>();
            rec.y = row["y"].as<int>();
            rec.type = row["type"].is_null() ? "" : row["type"].as<std::string>();
            rec.tmpl = row["template_id"].is_null() ? "" : row["template_id"].as<std::string>();
            rec.ov = json::object();
            if (!row["override_json"].is_null()) {
                const std::string raw = row["override_json"].as<std::string>();
                if (!raw.empty()) {
                    try { rec.ov = json::parse(raw); } catch (...) {}
                }
            }
            rec.respawnTime = static_cast<float>(row["respawn_time"].as<double>());
            rec.respawning = row["is_respawning"].as<bool>();
            spawns.push_back(rec);
        }

        if (!spawns.empty()) {
            int w = outRoom.value("width", 0);
            int h = outRoom.value("height", 0);
            if (w > 0 && h > 0) {
                std::vector<std::string> grid(h, std::string(w, '.'));
                json legend = json::object();
                std::map<std::string, char> keyToChar;
                const char* alphabet = "gmOXCsnrpqtuvwzyabcdefhijkABCDEFGHIJKLMNPQRSTUVWXYZ";
                int alphaIdx = 0;
                for (auto& rec : spawns) {
                    std::string key = rec.type + ":" + rec.tmpl;
                    if (!keyToChar.count(key)) {
                        keyToChar[key] = (alphaIdx < (int)strlen(alphabet)) ? alphabet[alphaIdx++] : '?';
                    }
                    char ch = keyToChar[key];
                    if (rec.x >= 0 && rec.x < w && rec.y >= 0 && rec.y < h) {
                        grid[rec.y][rec.x] = ch;
                    }
                }
                for (auto& kv : keyToChar) {
                    for (auto& rec : spawns) {
                        std::string key = rec.type + ":" + rec.tmpl;
                        if (key == kv.first) {
                            json entry = json::object();
                            entry["type"] = rec.type;
                            entry["id"] = rec.tmpl;
                            entry["respawn_time"] = rec.respawnTime;
                            entry["respawn"] = rec.respawning;
                            if (!rec.ov.is_null() && !rec.ov.empty()) entry["overrides"] = rec.ov;
                            std::string k(1, kv.second);
                            legend[k] = entry;
                            break;
                        }
                    }
                }
                json spawnRows = json::array();
                for (const auto& row : grid) spawnRows.push_back(row);
                outRoom["spawns"] = spawnRows;
                outRoom["spawn_legend"] = legend;
            }
        }

        tx.commit();
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Postgres] LoadRoomJson failed: %s\n", e.what());
        return false;
    }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
