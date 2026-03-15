#include "SQLiteDatabase.h"
#include <stdio.h>
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
    InitializeSchema();
    return true;
}

void SQLiteDatabase::Disconnect() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}
void SQLiteDatabase::InitializeSchema() {
    char* errMsg = 0;

    // We added 'account_id' to link to your PlayerComponent 
    // and 'body_mods' to store the JSON for Mutations/Bloodlines.
    const char* sql =
        "CREATE TABLE IF NOT EXISTS players ("
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
        "item_state TEXT NOT NULL," // Stores { "equipped": bool, "upgrade": int, "slot": int }
        "FOREIGN KEY (owner_id) REFERENCES players(id) ON DELETE CASCADE"
        ");";

    int rc = sqlite3_exec(db, sql, 0, 0, &errMsg);

    if (rc != SQLITE_OK) {
        LogError(errMsg);
        sqlite3_free(errMsg);
    }
    else {
        printf("[Database] Schema check complete. Body Mods and Account IDs enabled.\n");
    }
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

    // 1. Pack all player data into JSON
    nlohmann::json playerData;
    
    // Stats object
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
    
    // Body mods if present
    if (body) {
        playerData["body_mods"] = nlohmann::json::array();
        for (auto const& [slot, mod] : body->activeMutations) {
            playerData["body_mods"].push_back({ {"slot", (int)slot}, {"id", mod.name} });
        }
    }

    // Player variables (addon data) if present
    auto* vars = ctx.registry->GetComponent<PlayerVariablesComponent>(playerEnt);
    if (vars) {
        playerData["variables"] = {
            {"intVars", vars->intVars},
            {"stringVars", vars->stringVars}
        };
    }

    // 2. Update Database
    const char* sql = "UPDATE players SET data = ?, room_id = ?, region_id = ? WHERE id = ?;";
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

    // 3. Delegate inventory save
    SaveInventory(playerEnt, ctx);

    return true;
}

int SQLiteDatabase::CreatePlayerRow(const std::string& name, const std::string& password, const std::string& salt) {
    const char* sql = "INSERT INTO players (permission, region_id, name, password_hash, salt, room_id, data) VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    int newId = -1;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        // Initial default data structure with stats object
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

        sqlite3_bind_int(stmt, 1, 50); // Normal Player Permission
        sqlite3_bind_text(stmt, 2, "floor1", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, password.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, salt.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 6, 1); // Starting room
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

    // Delete old items
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

    // Lambda to save an item
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
    const char* sql = "SELECT id, region_id, room_id, permission, data FROM players WHERE name = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        outData.id = sqlite3_column_int(stmt, 0);
        outData.region = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        outData.room_id = sqlite3_column_int(stmt, 2);
        outData.permission = sqlite3_column_int(stmt, 3);
        outData.name = name;

        // Parse the JSON data blob
        std::string dataRaw = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        outData.data = nlohmann::json::parse(dataRaw);

        // Fetch items associated with this player
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
    const char* sql = "SELECT * FROM players WHERE name = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        return true;
    }

    else return false;

}

bool SQLiteDatabase::UpdatePassword(const std::string& name, const std::string& passwordHash, const std::string& salt) {
    const char* sql = "UPDATE players SET password_hash = ?, salt = ? WHERE name = ?;";
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
    const char* sql = "SELECT password_hash, salt FROM players WHERE name = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

    bool verified = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string storedHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string salt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        
        // Recreate the hash with the provided password and stored salt
        std::string combined = password + salt;
        std::string computedHash = picosha2::hash256_hex_string(combined);
        
        verified = (computedHash == storedHash);
    }
    
    sqlite3_finalize(stmt);
    return verified;
}
