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

    void BeginTransaction();
    void EndTransaction();

    bool SavePlayer(EntityID playerEnt, GameContext& ctx) override;
    bool LoadPlayer(const std::string& name, PlayerData& outData) override;
    bool PlayerExists(const std::string& name) override;
    int  CreatePlayerRow(const std::string& name,
                         const std::string& passwordHash,
                         const std::string& salt);

    bool UpdatePassword(const std::string& name, const std::string& passwordHash, const std::string& salt) override;
    bool VerifyPassword(const std::string& name, const std::string& password) override;

    bool LoadTerrain();
    nlohmann::json LoadItems(const std::string& worldId);
    nlohmann::json LoadMobs(const std::string& worldId);
    nlohmann::json LoadInteractables(const std::string& worldId);
    nlohmann::json LoadSkills(const std::string& worldId);
    nlohmann::json LoadLootTables(const std::string& worldId);
    nlohmann::json LoadDialogues(const std::string& worldId);

    bool RegionExists(const std::string& worldId, const std::string& regionId);
    bool LoadRegionFloorSettings(const std::string& worldId,
                                 const std::string& regionId,
                                 nlohmann::json& outSettings);
    std::vector<int> LoadRoomIds(const std::string& worldId, const std::string& regionId);
    bool LoadRoomJson(const std::string& worldId,
                      const std::string& regionId,
                      int roomId,
                      nlohmann::json& outRoom);

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
