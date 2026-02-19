#pragma once
#include <string>
#include "PlayerData.h"

struct GameContext;

class IDatabase {
public:
    virtual ~IDatabase() = default;

    // Lifecycle
    virtual bool Connect(const std::string& connectionString) = 0;
    virtual void Disconnect() = 0;

    // Gameplay Data
    virtual bool SavePlayer(int playerEnt, GameContext& ctx) = 0;
    virtual bool LoadPlayer(const std::string& name, PlayerData& outData) = 0;
    virtual bool PlayerExists(const std::string& name) = 0;

    // Password Management
    virtual bool UpdatePassword(const std::string& name, const std::string& passwordHash, const std::string& salt) = 0;
    virtual bool VerifyPassword(const std::string& name, const std::string& password) = 0;
};