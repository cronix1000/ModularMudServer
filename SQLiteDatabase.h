#pragma once
#include "IDatabase.h"
#include <sqlite3.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using EntityID = int;
struct GameContext;
struct PlayerData;
class SaveItemData;

class SQLiteDatabase : public IDatabase {
private:
    sqlite3* db = nullptr;

public:
    ~SQLiteDatabase();

    // Opens the file (creates it if missing) and initializes tables
    bool Connect(const std::string& filepath) override;
    void Disconnect() override;

    // Interface Implementation
    void SaveInventory(EntityID playerEnt, GameContext& ctx);
    bool LoadPlayer(const std::string& name, PlayerData& outData) override;
    std::vector<SavedItemData> GetSavedItems(int dbId);
    bool PlayerExists(const std::string& name) override;

    int CreatePlayerRow(const std::string& name, const std::string& passwordHash, const std::string& salt) override;
    bool SavePlayer(EntityID playerEnt, GameContext& ctx) override;
    void BeginTransaction() override;

    void EndTransaction() override;

    // Password Management
    bool UpdatePassword(const std::string& name, const std::string& passwordHash, const std::string& salt) override;
    bool VerifyPassword(const std::string& name, const std::string& password) override;

    // World loading from world_* tables. Each returns a JSON array shaped to match
    // the legacy JSON file the corresponding factory used to consume.
    bool LoadTerrain() override;
    nlohmann::json LoadItems(const std::string& worldId) override;
    nlohmann::json LoadMobs(const std::string& worldId) override;
    nlohmann::json LoadInteractables(const std::string& worldId) override;
    nlohmann::json LoadSkills(const std::string& worldId) override;
    nlohmann::json LoadLootTables(const std::string& worldId) override;
    nlohmann::json LoadDialogues(const std::string& worldId) override;

    // Region / room loading
    bool RegionExists(const std::string& worldId, const std::string& regionId) override;
    bool LoadRegionFloorSettings(const std::string& worldId, const std::string& regionId, nlohmann::json& outSettings) override;
    std::vector<int> LoadRoomIds(const std::string& worldId, const std::string& regionId) override;
    bool LoadRoomJson(const std::string& worldId, const std::string& regionId, int roomId, nlohmann::json& outRoom) override;

private:
    // Helper to run the CREATE TABLE sql
    void InitializeSchema();

    // One-shot copy of any main.player_* rows into players.player_*, then
    // drop the originals. No-op if the world DB has no player rows or the
    // players DB already contains them.
    void MigratePlayersFromMain();

    void SeedDefaultPlayerIfEmpty();

    // Helper for error logging
    void LogError(const char* message);

    // Internal: turn a prepared statement row into a JSON object.
    // jsonColumns are parsed; if the column ends in "_json" and contains an object,
    // its keys are merged into the parent. If it contains an array, it is stored under
    // the column name with "_json" stripped. skipColumns are not emitted at all.
    static nlohmann::json RowToJson(sqlite3_stmt* stmt,
                                     const std::vector<std::string>& skipColumns = {},
                                     const std::vector<std::string>& jsonColumns = {});
};
