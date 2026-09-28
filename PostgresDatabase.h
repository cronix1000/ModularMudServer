#pragma once
#include "IDatabase.h"
#include <pqxx/pqxx>
#include <memory>
#include <string>
#include <vector>

using EntityID = int;
struct GameContext;

class PostgresDatabase : public IDatabase {
public:
    PostgresDatabase();
    ~PostgresDatabase() override;

    // IDatabase
    bool Connect(const std::string& connectionString) override;
    void Disconnect() override;

    void BeginTransaction() override;
    void EndTransaction() override;

    bool SavePlayer(EntityID playerEnt, GameContext& ctx) override;
    bool LoadPlayer(const std::string& name, PlayerData& outData) override;
    bool PlayerExists(const std::string& name) override;
    int  CreatePlayerRow(const std::string& name,
                         const std::string& passwordHash,
                         const std::string& salt) override;

    bool UpdatePassword(const std::string& name, const std::string& passwordHash, const std::string& salt) override;
    bool VerifyPassword(const std::string& name, const std::string& password) override;

    bool LoadTerrain() override;
    nlohmann::json LoadItems(const std::string& worldId) override;
    nlohmann::json LoadMobs(const std::string& worldId) override;
    nlohmann::json LoadInteractables(const std::string& worldId) override;
    nlohmann::json LoadSkills(const std::string& worldId) override;
    nlohmann::json LoadLootTables(const std::string& worldId) override;
    nlohmann::json LoadDialogues(const std::string& worldId) override;

    bool RegionExists(const std::string& worldId, const std::string& regionId) override;
    bool LoadRegionFloorSettings(const std::string& worldId,
                                 const std::string& regionId,
                                 nlohmann::json& outSettings) override;
    std::vector<int> LoadRoomIds(const std::string& worldId, const std::string& regionId) override;
    bool LoadRoomJson(const std::string& worldId,
                      const std::string& regionId,
                      int roomId,
                      nlohmann::json& outRoom) override;

private:
    std::unique_ptr<pqxx::connection> conn;

    // Active transaction (if any). libpqxx nests txns naturally — we
    // open a pqxx::work in BeginTransaction() and commit/abort it in
    // EndTransaction(). Null when no transaction is in flight.
    std::unique_ptr<pqxx::work> activeTx;

    // JSON row helpers. Mirrors SQLiteDatabase::RowToJson: jsonColumns are
    // parsed; columns ending in "_json" that hold an object are merged into
    // the parent; array values are stored under the column name with the
    // "_json" suffix stripped. skipColumns are omitted.
    static nlohmann::json RowToJson(const pqxx::row& row,
                                    const std::vector<std::string>& skipColumns = {},
                                    const std::vector<std::string>& jsonColumns = {});

    // Internal: persist a player's inventory. Used by SavePlayer(); matches
    // SQLiteDatabase::SaveInventory's "delete then insert" pattern.
    void SaveInventory(EntityID playerEnt, GameContext& ctx);

    static void LogError(const char* message);
};
