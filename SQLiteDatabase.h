#pragma once
#include "IDatabase.h"
#include <sqlite3.h>

using EntityID = int;
struct GameContext;
class PlayerData;
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

    int CreatePlayerRow(const std::string& name, const std::string& passwordHash, const std::string& salt);
    bool SavePlayer(EntityID playerEnt, GameContext& ctx) override;
    bool SaveStats(EntityID playerEnt, GameContext& ctx);
    bool SaveItems(EntityID playerEnt, GameContext& ctx);
    bool SaveBodyMods(EntityID playerEnt, GameContext& ctx);
    void BeginTransaction();

    void EndTransaction();

private:
    // Helper to run the CREATE TABLE sql
    void InitializeSchema();


    // Helper for error logging
    void LogError(const char* message);
};