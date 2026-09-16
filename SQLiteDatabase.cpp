#include "SQLiteDatabase.h"
#include "DatabaseAttach.h"
#include <stdio.h>
#include <iostream>
#include <cstring>
#include <cstdlib>
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

SQLiteDatabase::~SQLiteDatabase() {
    Disconnect();
}

bool SQLiteDatabase::Connect(const std::string& filepath) {
    int rc = sqlite3_open(filepath.c_str(), &db);
    if (rc) {
        LogError("Can't open database");
        return false;
    }
    printf("[Database] Connected to %s\n", filepath.c_str());

    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    InitializeSchema();

    const char* envPlayers = std::getenv("MUD_PLAYERS_DB");
    const bool splitEnabled = envPlayers && envPlayers[0] != '\0';
    const std::string playersPath = splitEnabled
                                        ? std::string(envPlayers)
                                        : DatabaseAttach::DerivePlayersPath(filepath);

    const bool hasAnyPlayerRows = [] (sqlite3* db) -> bool {
        if (!db) return false;
        const char* tables[] = {"player_players", "player_items", "player_known_recipes"};
        for (const char* t : tables) {
            std::string q = "select exists(select 1 from ";
            q += t;
            q += ");";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) != SQLITE_OK) continue;
            int hit = 0;
            if (sqlite3_step(stmt) == SQLITE_ROW) hit = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
            if (hit) return true;
        }
        return false;
    }(db);

    if (DatabaseAttach::AttachPlayers(db, playersPath)) {
        printf("[Database] Players DB attached: %s\n", playersPath.c_str());

        bool needsMigrate = false;
        {
            const std::string q =
                std::string("select name from ") + DatabaseAttach::kPlayersDbAlias +
                ".sqlite_master where type='table' and name in "
                "('player_players','player_items','player_known_recipes')";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, q.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                int found = 0;
                while (sqlite3_step(stmt) == SQLITE_ROW) ++found;
                sqlite3_finalize(stmt);
                needsMigrate = found < 3;
            }
        }

        if (hasAnyPlayerRows && needsMigrate) {
            printf("[Database] Migrating player_* rows from main -> players ...\n");
            sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

            sqlite3_exec(db,
                "INSERT OR IGNORE INTO players.player_players "
                "(id, region_id, account_id, permission, name, password_hash, salt, room_id, data) "
                "SELECT id, region_id, account_id, permission, name, password_hash, salt, room_id, data "
                "FROM main.player_players",
                nullptr, nullptr, nullptr);

            sqlite3_exec(db,
                "INSERT OR IGNORE INTO players.player_items "
                "(id, owner_id, template_id, item_state) "
                "SELECT id, owner_id, template_id, item_state FROM main.player_items",
                nullptr, nullptr, nullptr);

            sqlite3_exec(db,
                "INSERT OR IGNORE INTO players.player_known_recipes "
                "(uid, world_id, recipe_id, learned_at) "
                "SELECT uid, world_id, recipe_id, learned_at FROM main.player_known_recipes",
                nullptr, nullptr, nullptr);

            sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);

            sqlite3_exec(db, "DROP TABLE IF EXISTS main.player_known_recipes;", nullptr, nullptr, nullptr);
            sqlite3_exec(db, "DROP TABLE IF EXISTS main.player_items;",       nullptr, nullptr, nullptr);
            sqlite3_exec(db, "DROP TABLE IF EXISTS main.player_players;",      nullptr, nullptr, nullptr);

            printf("[Database] Player migration complete; main.player_* dropped.\n");
        } else if (needsMigrate) {
            sqlite3_exec(db,
                "INSERT OR IGNORE INTO players.player_players "
                "(id, region_id, account_id, permission, name, password_hash, salt, room_id, data) "
                "SELECT id, region_id, account_id, permission, name, password_hash, salt, room_id, data "
                "FROM main.player_players",
                nullptr, nullptr, nullptr);
            sqlite3_exec(db,
                "INSERT OR IGNORE INTO players.player_items "
                "(id, owner_id, template_id, item_state) "
                "SELECT id, owner_id, template_id, item_state FROM main.player_items",
                nullptr, nullptr, nullptr);
            sqlite3_exec(db,
                "INSERT OR IGNORE INTO players.player_known_recipes "
                "(uid, world_id, recipe_id, learned_at) "
                "SELECT uid, world_id, recipe_id, learned_at FROM main.player_known_recipes",
                nullptr, nullptr, nullptr);
            sqlite3_exec(db, "DROP TABLE IF EXISTS main.player_known_recipes;", nullptr, nullptr, nullptr);
            sqlite3_exec(db, "DROP TABLE IF EXISTS main.player_items;",       nullptr, nullptr, nullptr);
            sqlite3_exec(db, "DROP TABLE IF EXISTS main.player_players;",      nullptr, nullptr, nullptr);
        }
    } else if (splitEnabled) {
        printf("[Database] WARNING: MUD_PLAYERS_DB was set but ATTACH failed; "
               "running in single-file mode against %s\n", filepath.c_str());
    }

    return true;
}

void SQLiteDatabase::Disconnect() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

void SQLiteDatabase::InitializeSchema() {
    char* errMsg = nullptr;

    const char* sql =
        "CREATE TABLE IF NOT EXISTS player_players ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "region_id TEXT DEFAULT 'floor1',"
        "account_id INTEGER UNIQUE,"
        "permission INTEGER NOT NULL,"
        "name TEXT UNIQUE NOT NULL,"
        "password_hash TEXT NOT NULL,"
        "salt TEXT NOT NULL,"
        "room_id INTEGER DEFAULT 1,"
        "data TEXT NOT NULL"
        ");"

        "CREATE TABLE IF NOT EXISTS player_items ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "owner_id INTEGER NOT NULL,"
        "template_id TEXT NOT NULL,"
        "item_state TEXT NOT NULL,"
        "FOREIGN KEY (owner_id) REFERENCES player_players(id) ON DELETE CASCADE"
        ");";

    int rc = sqlite3_exec(db, sql, 0, 0, &errMsg);

    if (rc != SQLITE_OK) {
        LogError(errMsg);
        sqlite3_free(errMsg);
    }
    else {
        printf("[Database] Schema check complete. Body Mods and Account IDs enabled.\n");
    }

    // SeedDefaultPlayerIfEmpty(); // disabled for test
}

void SQLiteDatabase::SeedDefaultPlayerIfEmpty() { /* disabled */ 
}

void SQLiteDatabase::LogError(const char* message)
{
    fprintf(stderr, "[Database Error] %s\n", message ? message : "Unknown error");
}

void SQLiteDatabase::BeginTransaction() {
    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
}

void SQLiteDatabase::EndTransaction() {
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
}

bool SQLiteDatabase::SavePlayer(EntityID playerEnt, GameContext& ctx) {
    auto* stats = ctx.registry->GetComponent<StatComponent>(playerEnt);
    auto* pos = ctx.registry->GetComponent<PositionComponent>(playerEnt);
    auto* playerComp = ctx.registry->GetComponent<PlayerComponent>(playerEnt);
    auto* body = ctx.registry->GetComponent<BodyComponent>(playerEnt);
    auto* region = ctx.registry->GetComponent<RegionComponent>(playerEnt);

    if (!stats || !pos || !playerComp) return false;

    nlohmann::json playerData;

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
        playerData["body_mods"] = nlohmann::json::array();
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

    const char* sql = "UPDATE player_players SET data = ?, room_id = ?, region_id = ? WHERE id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        std::string dataStr = playerData.dump();
        sqlite3_bind_text(stmt, 1, dataStr.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, pos->roomId);
        sqlite3_bind_text(stmt, 3, region->region.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, playerComp->accountID);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    SaveInventory(playerEnt, ctx);

    return true;
}

int SQLiteDatabase::CreatePlayerRow(const std::string& name, const std::string& password, const std::string& salt) {
    const char* sql = "INSERT INTO player_players (permission, region_id, name, password_hash, salt, room_id, data) VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    int newId = -1;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        nlohmann::json defaultData;
        defaultData["stats"] = {
            {"hp", 100},
            {"max_hp", 100},
            {"str", 10},
            {"dex", 10},
            {"int", 10},
            {"wis", 10},
            {"atk", 5},
            {"atkspd", 1.0},
            {"mana", 50}
        };
        std::string dataStr = defaultData.dump();

        sqlite3_bind_int(stmt, 1, 50);
        sqlite3_bind_text(stmt, 2, "floor1", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, password.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, salt.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 6, 1);
        sqlite3_bind_text(stmt, 7, dataStr.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            newId = (int)sqlite3_last_insert_rowid(db);
        }
        sqlite3_finalize(stmt);
    }
    return newId;
}

void SQLiteDatabase::SaveInventory(EntityID playerEnt, GameContext& ctx) {
    auto playerComp = ctx.registry->GetComponent<PlayerComponent>(playerEnt);
    if (!playerComp) return;

    BeginTransaction();
    std::string delSql = "DELETE FROM player_items WHERE owner_id = ?;";
    sqlite3_stmt* delStmt;
    if (sqlite3_prepare_v2(this->db, delSql.c_str(), -1, &delStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(delStmt, 1, playerComp->accountID);
        sqlite3_step(delStmt);
        sqlite3_finalize(delStmt);
    }

    auto inv = ctx.registry->GetComponent<InventoryComponent>(playerEnt);
    auto equip = ctx.registry->GetComponent<EquipmentComponent>(playerEnt);

    auto saveItemLambda = [this, &ctx, playerComp](EntityID itemEnt, bool isEquipped, int slotID) {
        auto item = ctx.registry->GetComponent<ItemComponent>(itemEnt);
        if (!item) return;

        nlohmann::json state;
        state["equipped"] = isEquipped;
        state["slot"] = slotID;

        const char* insSql = "INSERT INTO player_items (owner_id, template_id, item_state) VALUES (?, ?, ?);";
        sqlite3_stmt* insStmt;
        if (sqlite3_prepare_v2(this->db, insSql, -1, &insStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(insStmt, 1, playerComp->accountID);
            sqlite3_bind_text(insStmt, 2, item->templateName.c_str(), -1, SQLITE_TRANSIENT);
            std::string stateStr = state.dump();
            sqlite3_bind_text(insStmt, 3, stateStr.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(insStmt);
            sqlite3_finalize(insStmt);
        }
        };

    if (inv) {
        for (EntityID id : inv->items) {
            saveItemLambda(id, false, -1);
        }
    }
    if (equip) {
        for (const auto& slotPair : equip->slots) {
            saveItemLambda(slotPair.second, true, static_cast<int>(slotPair.first));
        }
    }

    EndTransaction();
}

bool SQLiteDatabase::LoadPlayer(const std::string& name, PlayerData& outData) {
    const char* sql = "SELECT id, region_id, room_id, permission, data FROM player_players WHERE name = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        outData.id = sqlite3_column_int(stmt, 0);
        outData.region = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        outData.room_id = sqlite3_column_int(stmt, 2);
        outData.permission = sqlite3_column_int(stmt, 3);
        outData.name = name;

        std::string dataRaw = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        outData.data = nlohmann::json::parse(dataRaw);

        outData.items = GetSavedItems(outData.id);

        sqlite3_finalize(stmt);
        return true;
    }

    sqlite3_finalize(stmt);
    return false;
}

std::vector<SavedItemData> SQLiteDatabase::GetSavedItems(int dbId) {
    std::vector<SavedItemData> items;
    const char* sql = "SELECT template_id, item_state FROM player_items WHERE owner_id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, dbId);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            SavedItemData item;
            item.templateId = (const char*)sqlite3_column_text(stmt, 0);
            item.state = nlohmann::json::parse((const char*)sqlite3_column_text(stmt, 1));
            items.push_back(item);
        }
        sqlite3_finalize(stmt);
    }
    return items;
}

bool SQLiteDatabase::PlayerExists(const std::string& name)
{
    const char* sql = "SELECT * FROM player_players WHERE name = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        return true;
    }

    else return false;

}

bool SQLiteDatabase::UpdatePassword(const std::string& name, const std::string& passwordHash, const std::string& salt) {
    const char* sql = "UPDATE player_players SET password_hash = ?, salt = ? WHERE name = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, passwordHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, salt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool SQLiteDatabase::VerifyPassword(const std::string& name, const std::string& password) {
    const char* sql = "SELECT password_hash, salt FROM player_players WHERE name = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

    bool verified = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string storedHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string salt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

        std::string combined = password + salt;
        std::string computedHash = picosha2::hash256_hex_string(combined);

        verified = (computedHash == storedHash);
    }

    sqlite3_finalize(stmt);
    return verified;
}

// ============================================================================
// World loading helpers
// ============================================================================

nlohmann::json SQLiteDatabase::RowToJson(sqlite3_stmt* stmt,
                                          const std::vector<std::string>& skipColumns,
                                          const std::vector<std::string>& jsonColumns) {
    nlohmann::json out = nlohmann::json::object();
    int colCount = sqlite3_column_count(stmt);

    auto shouldSkip = [&](const std::string& name) {
        for (const auto& s : skipColumns) if (s == name) return true;
        return false;
    };
    auto isJsonCol = [&](const std::string& name) {
        for (const auto& s : jsonColumns) if (s == name) return true;
        return false;
    };

    for (int i = 0; i < colCount; ++i) {
        const char* colName = sqlite3_column_name(stmt, i);
        std::string name = colName ? colName : "";
        if (shouldSkip(name)) continue;

        int type = sqlite3_column_type(stmt, i);
        if (type == SQLITE_NULL) continue;

        switch (type) {
        case SQLITE_INTEGER:
            out[name] = sqlite3_column_int64(stmt, i);
            break;
        case SQLITE_FLOAT:
            out[name] = (double)sqlite3_column_double(stmt, i);
            break;
        case SQLITE_TEXT: {
            const unsigned char* txt = sqlite3_column_text(stmt, i);
            std::string s = txt ? reinterpret_cast<const char*>(txt) : "";
            if (isJsonCol(name) && !s.empty()) {
                try {
                    nlohmann::json parsed = nlohmann::json::parse(s);
                    if (parsed.is_object()) {
                        for (auto it = parsed.begin(); it != parsed.end(); ++it) {
                            out[it.key()] = it.value();
                        }
                    } else if (parsed.is_array()) {
                        std::string baseName = name;
                        const std::string suffix = "_json";
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
            break;
        }
        default:
            break;
        }
    }
    return out;
}

bool SQLiteDatabase::LoadTerrain() {
    const char* sql = "SELECT symbol, name, color, blocks_move, blocks_sight, move_cost "
                      "FROM world_terrains;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LogError("LoadTerrain prepare failed");
        return false;
    }
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* symTxt = sqlite3_column_text(stmt, 0);
        if (!symTxt) continue;
        char symbol = (char)symTxt[0];
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* color = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        bool blocksMove = sqlite3_column_int(stmt, 3) != 0;
        bool blocksSight = sqlite3_column_int(stmt, 4) != 0;
        int moveCost = sqlite3_column_int(stmt, 5);
        globalTerrain[symbol] = {
            symbol,
            name ? name : "Unknown",
            color ? color : "white",
            blocksMove,
            blocksSight,
            moveCost
        };
        ++count;
    }
    sqlite3_finalize(stmt);
    printf("[Database] Loaded %d terrain symbols from world_terrains.\n", count);
    return true;
}

nlohmann::json SQLiteDatabase::LoadItems(const std::string& worldId) {
    nlohmann::json out = nlohmann::json::object();
    const char* sql =
        "SELECT template_id, name, description, char, color, value, weight, equippable, type, "
        "       components_json, script_ref "
        "FROM world_items WHERE world_id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_text(stmt, 1, worldId.c_str(), -1, SQLITE_TRANSIENT);
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* idTxt = sqlite3_column_text(stmt, 0);
        if (!idTxt) continue;
        std::string key = reinterpret_cast<const char*>(idTxt);
        nlohmann::json obj = RowToJson(stmt, {"template_id"}, {"components_json"});
        if (obj.contains("script_ref")) {
            obj["script"] = obj["script_ref"];
        }
        // Convert integer 0/1 boolean columns to actual JSON booleans
        // so ItemFactory's .value<bool>() calls don't throw type_error.
        if (obj.contains("equippable") && obj["equippable"].is_number_integer()) {
            obj["equippable"] = obj["equippable"].get<int>() != 0;
        }
        // components_json was merged into obj; move any component-like keys under "components"
        // so ItemFactory sees the same shape as items.json.
        static const std::vector<std::string> componentKeys = {
            "weapon", "armour", "scripts", "passive_skills", "armor"
        };
        bool hasComponents = false;
        nlohmann::json componentsObj = nlohmann::json::object();
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
    sqlite3_finalize(stmt);
    printf("[Database] Loaded %d items from world_items.\n", count);
    return out;
}

nlohmann::json SQLiteDatabase::LoadMobs(const std::string& worldId) {
    nlohmann::json out = nlohmann::json::object();
    const char* sql =
        "SELECT template_id, name, description, char, color, hp, level, ai, loot_drop, "
        "       strength, dexterity, intelligence, attack_damage, attack_speed, crit_chance, "
        "       crit_mult, attack_patterns_json, script_ref, extra_json "
        "FROM world_mobs WHERE world_id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_text(stmt, 1, worldId.c_str(), -1, SQLITE_TRANSIENT);
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* idTxt = sqlite3_column_text(stmt, 0);
        if (!idTxt) continue;
        std::string key = reinterpret_cast<const char*>(idTxt);
        nlohmann::json obj = RowToJson(stmt, {"template_id"}, {"attack_patterns_json", "extra_json"});
        if (obj.contains("script_ref")) obj["script"] = obj["script_ref"];
        // MobFactory expects stats nested as "stat": {strength, dexterity, intelligence}
        nlohmann::json stat = nlohmann::json::object();
        if (obj.contains("strength"))   { stat["strength"] = obj["strength"];   obj.erase("strength"); }
        if (obj.contains("dexterity"))  { stat["dexterity"] = obj["dexterity"]; obj.erase("dexterity"); }
        if (obj.contains("intelligence")) { stat["intelligence"] = obj["intelligence"]; obj.erase("intelligence"); }
        obj["stat"] = stat;
        // crit_chance / crit_mult -> critical_chance / critical_multiplier (existing JSON keys)
        if (obj.contains("crit_chance") && !obj.contains("critical_chance")) {
            obj["critical_chance"] = obj["crit_chance"]; obj.erase("crit_chance");
        }
        if (obj.contains("crit_mult") && !obj.contains("critical_multiplier")) {
            obj["critical_multiplier"] = obj["crit_mult"]; obj.erase("crit_mult");
        }
        out[key] = obj;
        ++count;
    }
    sqlite3_finalize(stmt);
    printf("[Database] Loaded %d mobs from world_mobs.\n", count);
    return out;
}

nlohmann::json SQLiteDatabase::LoadInteractables(const std::string& worldId) {
    nlohmann::json out = nlohmann::json::object();
    const char* sql =
        "SELECT template_id, name, description, char, color, components_json, script_ref "
        "FROM world_interactables WHERE world_id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_text(stmt, 1, worldId.c_str(), -1, SQLITE_TRANSIENT);
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* idTxt = sqlite3_column_text(stmt, 0);
        if (!idTxt) continue;
        std::string key = reinterpret_cast<const char*>(idTxt);
        nlohmann::json obj = RowToJson(stmt, {"template_id"}, {"components_json"});
        if (obj.contains("script_ref")) obj["script"] = obj["script_ref"];
        // components_json was merged into obj; wrap any keys under "components"
        // so InteractableFactory sees the same shape as interactables.json.
        static const std::vector<std::string> knownTypes = {
            "portal", "chest", "door", "lever", "healing", "inventory", "loot"
        };
        nlohmann::json componentsObj = nlohmann::json::object();
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
    sqlite3_finalize(stmt);
    printf("[Database] Loaded %d interactables from world_interactables.\n", count);
    return out;
}

nlohmann::json SQLiteDatabase::LoadSkills(const std::string& worldId) {
    nlohmann::json out = nlohmann::json::object();
    nlohmann::json categories = nlohmann::json::object();
    nlohmann::json skills = nlohmann::json::object();

    const char* catSql =
        "SELECT category_id, name, description, stats_json, synergy_bonus "
        "FROM world_skill_categories WHERE world_id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, catSql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_text(stmt, 1, worldId.c_str(), -1, SQLITE_TRANSIENT);
    int catCount = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* idTxt = sqlite3_column_text(stmt, 0);
        if (!idTxt) continue;
        std::string key = reinterpret_cast<const char*>(idTxt);
        nlohmann::json obj = RowToJson(stmt, {"category_id"}, {"stats_json"});
        if (obj.contains("synergy_bonus") && !obj.contains("synergyBonus")) {
            obj["synergyBonus"] = obj["synergy_bonus"]; obj.erase("synergy_bonus");
        }
        categories[key] = obj;
        ++catCount;
    }
    sqlite3_finalize(stmt);

    const char* skSql =
        "SELECT skill_id, category_id, name, description, type, activation, command, "
        "       cooldown, windup, costs_json, targeting, range, script_ref "
        "FROM world_skills WHERE world_id = ?;";
    if (sqlite3_prepare_v2(db, skSql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_text(stmt, 1, worldId.c_str(), -1, SQLITE_TRANSIENT);
    int skillCount = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* idTxt = sqlite3_column_text(stmt, 0);
        if (!idTxt) continue;
        std::string key = reinterpret_cast<const char*>(idTxt);
        nlohmann::json obj = RowToJson(stmt, {"skill_id"}, {"costs_json"});
        if (obj.contains("script_ref")) obj["script"] = obj["script_ref"];
        skills[key] = obj;
        ++skillCount;
    }
    sqlite3_finalize(stmt);

    out["skill_categories"] = categories;
    out["skills"] = skills;
    printf("[Database] Loaded %d skill categories and %d skills from world_skills.\n", catCount, skillCount);
    return out;
}

nlohmann::json SQLiteDatabase::LoadLootTables(const std::string& worldId) {
    nlohmann::json out = nlohmann::json::object();
    const char* sql =
        "SELECT table_id, name, entries_json "
        "FROM world_loot_tables WHERE world_id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_text(stmt, 1, worldId.c_str(), -1, SQLITE_TRANSIENT);
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* idTxt = sqlite3_column_text(stmt, 0);
        if (!idTxt) continue;
        std::string key = reinterpret_cast<const char*>(idTxt);
        nlohmann::json obj = RowToJson(stmt, {"table_id"}, {"entries_json"});
        // LootFactory expects entries to be an array of {id, weight}. entries_json is the array.
        out[key] = obj.contains("entries") ? obj["entries"] : nlohmann::json::array();
        ++count;
    }
    sqlite3_finalize(stmt);
    printf("[Database] Loaded %d loot tables from world_loot_tables.\n", count);
    return out;
}

nlohmann::json SQLiteDatabase::LoadDialogues(const std::string& worldId) {
    nlohmann::json out = nlohmann::json::object();
    const char* sql =
        "SELECT node_id, text, idle_json, combat_json, death_json, options_json "
        "FROM world_dialogues WHERE world_id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_text(stmt, 1, worldId.c_str(), -1, SQLITE_TRANSIENT);
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* idTxt = sqlite3_column_text(stmt, 0);
        if (!idTxt) continue;
        std::string key = reinterpret_cast<const char*>(idTxt);

        nlohmann::json obj = RowToJson(stmt, {"node_id"}, {"idle_json", "combat_json", "death_json", "options_json"});
        // DialogueFactory expects keys: text, options (array), idle, combat, death
        if (obj.contains("text") && !obj["text"].is_null()) {
            // dialogue node
            nlohmann::json node = nlohmann::json::object();
            node["text"] = obj["text"];
            if (obj.contains("options") && obj["options"].is_array()) {
                node["options"] = obj["options"];
            } else {
                node["options"] = nlohmann::json::array();
            }
            out[key] = node;
        } else {
            // voice bark
            nlohmann::json vs = nlohmann::json::object();
            if (obj.contains("idle"))    vs["idle"] = obj["idle"];
            if (obj.contains("combat"))  vs["combat"] = obj["combat"];
            if (obj.contains("death"))   vs["death"] = obj["death"];
            out[key] = vs;
        }
        ++count;
    }
    sqlite3_finalize(stmt);
    printf("[Database] Loaded %d dialogue entries from world_dialogues.\n", count);
    return out;
}

bool SQLiteDatabase::RegionExists(const std::string& worldId, const std::string& regionId) {
    const char* sql = "SELECT 1 FROM world_regions WHERE world_id = ? AND id = ? LIMIT 1;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, worldId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, regionId.c_str(), -1, SQLITE_TRANSIENT);
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

bool SQLiteDatabase::LoadRegionFloorSettings(const std::string& worldId, const std::string& regionId, nlohmann::json& outSettings) {
    outSettings = nlohmann::json::object();
    const char* sql = "SELECT floor_settings_json FROM world_regions WHERE world_id = ? AND id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, worldId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, regionId.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* txt = sqlite3_column_text(stmt, 0);
        if (txt) {
            std::string raw = reinterpret_cast<const char*>(txt);
            if (!raw.empty()) {
                try {
                    outSettings = nlohmann::json::parse(raw);
                    ok = true;
                } catch (const std::exception& e) {
                    std::cerr << "[Database] floor_settings_json parse error for region " << regionId << ": " << e.what() << std::endl;
                }
            } else {
                ok = true;
            }
        } else {
            ok = true;
        }
    }
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<int> SQLiteDatabase::LoadRoomIds(const std::string& worldId, const std::string& regionId) {
    std::vector<int> ids;
    const char* sql =
        "SELECT room_id FROM world_rooms WHERE world_id = ? AND region_id = ? ORDER BY room_id;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return ids;
    sqlite3_bind_text(stmt, 1, worldId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, regionId.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ids.push_back(sqlite3_column_int(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return ids;
}

bool SQLiteDatabase::LoadRoomJson(const std::string& worldId, const std::string& regionId, int roomId, nlohmann::json& outRoom) {
    outRoom = nlohmann::json::object();

    const char* roomSql =
        "SELECT room_id, name, description, terrain, width, height, layout_json, "
        "       spawn_x, spawn_y, scripts_json, extra_json "
        "FROM world_rooms WHERE world_id = ? AND region_id = ? AND room_id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, roomSql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, worldId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, regionId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, roomId);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return false;
    }
    outRoom = RowToJson(stmt, {"world_id", "region_id", "room_id"}, {"layout_json", "scripts_json", "extra_json"});
    sqlite3_finalize(stmt);

    outRoom["id"] = roomId;

    nlohmann::json exits = nlohmann::json::object();
    const char* exitSql =
        "SELECT direction, to_room_id, dest_x, dest_y, is_one_way, is_portal, portal_name, auto_trigger "
        "FROM world_room_exits WHERE world_id = ? AND region_id = ? AND from_room_id = ?;";
    if (sqlite3_prepare_v2(db, exitSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, worldId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, regionId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, roomId);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* dirTxt = sqlite3_column_text(stmt, 0);
            if (!dirTxt) continue;
            std::string dir = reinterpret_cast<const char*>(dirTxt);
            nlohmann::json exitObj = nlohmann::json::object();
            exitObj["target_room"] = sqlite3_column_int(stmt, 1);
            exitObj["dest_x"] = sqlite3_column_int(stmt, 2);
            exitObj["dest_y"] = sqlite3_column_int(stmt, 3);
            exitObj["is_portal"] = sqlite3_column_int(stmt, 5) != 0;
            const unsigned char* pname = sqlite3_column_text(stmt, 6);
            if (pname) exitObj["portal_name"] = reinterpret_cast<const char*>(pname);
            exitObj["auto_trigger"] = sqlite3_column_int(stmt, 7) != 0;
            exits[dir] = exitObj;
        }
        sqlite3_finalize(stmt);
    }
    if (!exits.empty()) outRoom["exits"] = exits;

    const char* spawnSql =
        "SELECT x, y, type, template_id, override_json, respawn_time, is_respawning "
        "FROM world_room_spawns WHERE world_id = ? AND region_id = ? AND room_id = ?;";
    struct SpawnRec { int x; int y; std::string type; std::string tmpl; nlohmann::json ov; float respawnTime; bool respawning; };
    std::vector<SpawnRec> spawns;
    if (sqlite3_prepare_v2(db, spawnSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, worldId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, regionId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, roomId);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            SpawnRec rec;
            rec.x = sqlite3_column_int(stmt, 0);
            rec.y = sqlite3_column_int(stmt, 1);
            const unsigned char* tType = sqlite3_column_text(stmt, 2);
            const unsigned char* tId = sqlite3_column_text(stmt, 3);
            rec.type = tType ? reinterpret_cast<const char*>(tType) : "";
            rec.tmpl = tId ? reinterpret_cast<const char*>(tId) : "";
            rec.ov = nlohmann::json::object();
            const unsigned char* ovTxt = sqlite3_column_text(stmt, 4);
            if (ovTxt) {
                std::string raw = reinterpret_cast<const char*>(ovTxt);
                if (!raw.empty()) {
                    try { rec.ov = nlohmann::json::parse(raw); } catch (...) {}
                }
            }
            rec.respawnTime = (float)sqlite3_column_double(stmt, 5);
            rec.respawning = sqlite3_column_int(stmt, 6) != 0;
            spawns.push_back(rec);
        }
        sqlite3_finalize(stmt);
    }

    if (!spawns.empty()) {
        int w = outRoom.value("width", 0);
        int h = outRoom.value("height", 0);
        if (w > 0 && h > 0) {
            std::vector<std::string> grid(h, std::string(w, '.'));
            nlohmann::json legend = nlohmann::json::object();
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
                        nlohmann::json entry = nlohmann::json::object();
                        entry["type"] = rec.type;
                        entry["id"] = rec.tmpl;
                        entry["respawn_time"] = rec.respawnTime;
                        entry["respawn"] = rec.respawning;
                        if (!rec.ov.is_null() && !rec.ov.empty()) {
                            entry["overrides"] = rec.ov;
                        }
                        std::string k(1, kv.second);
                        legend[k] = entry;
                        break;
                    }
                }
            }
            outRoom["spawns"] = grid;
            outRoom["spawn_legend"] = legend;
        }
    }

    return true;
}
