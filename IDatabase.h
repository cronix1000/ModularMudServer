#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "PlayerData.h"

struct GameContext;

class IDatabase {
public:
    virtual ~IDatabase() = default;

    // Lifecycle
    virtual bool Connect(const std::string& connectionString) = 0;
    virtual void Disconnect() = 0;

    // Transaction control (was concrete on SQLiteDatabase; promoted so
    // PostgresDatabase can use libpqxx transactions uniformly).
    virtual void BeginTransaction() = 0;
    virtual void EndTransaction() = 0;

    // Gameplay Data
    virtual bool SavePlayer(int playerEnt, GameContext& ctx) = 0;
    virtual bool LoadPlayer(const std::string& name, PlayerData& outData) = 0;
    virtual bool PlayerExists(const std::string& name) = 0;
    virtual int  CreatePlayerRow(const std::string& name,
                                 const std::string& passwordHash,
                                 const std::string& salt) = 0;

    // Password Management
    virtual bool UpdatePassword(const std::string& name, const std::string& passwordHash, const std::string& salt) = 0;
    virtual bool VerifyPassword(const std::string& name, const std::string& password) = 0;

    // World loading (was concrete on SQLiteDatabase; promoted so FactoryManager
    // can call the right backend through the GameContext handle). Each loader
    // returns a JSON value shaped to match the legacy JSON files the factories
    // were originally written against — see SQLiteDatabase.cpp / PostgresDatabase.cpp
    // for the per-table column->key mapping rules.
    virtual bool LoadTerrain() = 0;
    virtual nlohmann::json LoadItems(const std::string& worldId) = 0;
    virtual nlohmann::json LoadMobs(const std::string& worldId) = 0;
    virtual nlohmann::json LoadInteractables(const std::string& worldId) = 0;
    virtual nlohmann::json LoadSkills(const std::string& worldId) = 0;
    virtual nlohmann::json LoadLootTables(const std::string& worldId) = 0;
    virtual nlohmann::json LoadDialogues(const std::string& worldId) = 0;

    // Region / room loading
    virtual bool RegionExists(const std::string& worldId, const std::string& regionId) = 0;
    virtual bool LoadRegionFloorSettings(const std::string& worldId,
                                         const std::string& regionId,
                                         nlohmann::json& outSettings) = 0;
    virtual std::vector<int> LoadRoomIds(const std::string& worldId, const std::string& regionId) = 0;
    virtual bool LoadRoomJson(const std::string& worldId,
                              const std::string& regionId,
                              int roomId,
                              nlohmann::json& outRoom) = 0;
};
