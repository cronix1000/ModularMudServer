#include "World.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include "ItemFactory.h"
#include "MobFactory.h"
#include "FactoryManager.h"
#include "GameContext.h"
#include "InteractableFactory.h"
#include "RespawnSystem.h"
#include "RoomFactory.h"
#include "Registry.h"
#include "SQLiteDatabase.h"

namespace fs = std::filesystem;

const fs::path REGION_DIR = "regions";
const std::string DEFAULT_WORLD_ID = "default";

namespace {
    bool ResolveRegionDirectory(const std::string& region, fs::path& outDir) {
        fs::path candidate = REGION_DIR / region;
        if (fs::is_directory(candidate)) {
            outDir = candidate;
            return true;
        }

        fs::path current = fs::current_path();
        while (true) {
            candidate = current / REGION_DIR / region;
            if (fs::is_directory(candidate)) {
                outDir = candidate;
                return true;
            }

            if (current == current.root_path()) break;
            current = current.parent_path();
        }

        return false;
    }
}

World::World()
{
}

World::~World()
{
}

Direction StringToDirection(const std::string& str) {
    if (str == "north") return Direction::North;
    if (str == "south") return Direction::South;
    if (str == "east")  return Direction::East;
    if (str == "west")  return Direction::West;
    if (str == "up") return Direction::Up;
    if (str == "down") return Direction::Down;
    return Direction::North;
}

bool World::CheckIfRegionLoaded(const std::string& regionId)
{
    if (loadedRegions.find(regionId) == loadedRegions.end()) {
        return false;
    }
    return true;
}

bool World::LoadRegion(const std::string& regionId, GameContext& ctx)
{
    if (CheckIfRegionLoaded(regionId))
        return true;

    if (!roomFactory) {
        roomFactory = new RoomFactory(ctx);
    }

    // Prefer DB. Fall back to filesystem regions/ only if region does not exist in DB.
    if (ctx.db && ctx.db->RegionExists(DEFAULT_WORLD_ID, regionId)) {
        nlohmann::json floorSettings;
        ctx.db->LoadRegionFloorSettings(DEFAULT_WORLD_ID, regionId, floorSettings);

        std::vector<int> roomIds = ctx.db->LoadRoomIds(DEFAULT_WORLD_ID, regionId);
        for (int roomId : roomIds) {
            nlohmann::json rData;
            if (!ctx.db->LoadRoomJson(DEFAULT_WORLD_ID, regionId, roomId, rData)) {
                std::cerr << "World::LoadRegion: failed to load room " << roomId << " for region " << regionId << std::endl;
                continue;
            }
            LoadRoomFromJson(rData, floorSettings, ctx);
        }

        loadedRegions.insert(regionId);
        return true;
    }

    // Legacy fallback (deprecated): walk regions/<id>/*.json files.
    fs::path regionDir;
    if (!ResolveRegionDirectory(regionId, regionDir)) {
        std::cerr << "World::LoadRegion: cannot find region '" << regionId << "' in DB or near "
            << fs::current_path() << std::endl;
        return false;
    }

    nlohmann::json floorSettings;
    fs::path settingsPath = regionDir / "floor_settings.json";
    if (fs::exists(settingsPath)) {
        std::ifstream sFile(settingsPath);
        try {
            sFile >> floorSettings;
        }
        catch (const nlohmann::json::parse_error& e) {
            std::cerr << "JSON Parse Error in " << settingsPath << ": " << e.what() << std::endl;
        }
    }

    try {
        for (const auto& entry : fs::directory_iterator(regionDir)) {
            if (!entry.is_regular_file()) continue;

            fs::path roomPath = entry.path();
            if (roomPath.extension() != ".json" ||
                roomPath.filename() == "floor_settings.json") continue;

            LoadRoomFile(roomPath.string(), floorSettings, ctx);
        }
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "World::LoadRegion: failed to read directory '" << regionDir << "': "
            << e.what() << std::endl;
        return false;
    }

    loadedRegions.insert(regionId);
    return true;
}

bool World::LoadRoomFile(const std::string& path, const json& floorSettings, GameContext& ctx)
{
    if (!roomFactory) {
        roomFactory = new RoomFactory(ctx);
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "World::LoadRoomFile: Failed to open " << path << std::endl;
        return false;
    }

    json rData;
    try {
        file >> rData;
    }
    catch (const json::parse_error& e) {
        std::cerr << "JSON Parse Error in " << path << ": " << e.what() << std::endl;
        return false;
    }

    if (rData.is_null()) {
        std::cerr << "World::LoadRoomFile: " << path << " contained no data" << std::endl;
        return false;
    }

    return LoadRoomFromJson(rData, floorSettings, ctx);
}

bool World::LoadRoomFromJson(const json& rData, const json& floorSettings, GameContext& ctx)
{
    int id = rData.value("id", -1);
    if (id < 0) {
        std::cerr << "World::LoadRoomFromJson: missing valid id" << std::endl;
        return false;
    }

    if (!roomFactory) {
        roomFactory = new RoomFactory(ctx);
    }

    EntityID roomEntity = roomFactory->CreateRoom(rData);
    if (roomEntity == 0) {
        std::cerr << "World::LoadRoomFromJson: Failed to create room id=" << id << std::endl;
        return false;
    }

    if (rData.contains("spawns") && rData.contains("spawn_legend")) {
        int logicalRoomId = rData.value("id", (int)roomEntity);
        ParseSpawns(rData, logicalRoomId, floorSettings, ctx);
    }

    return true;
}

void World::ParseSpawns(const json& rData, int roomID,const json& floorSettings, GameContext& ctx)
{
    json legend = rData["spawn_legend"];
    int y = 0;

    for (const std::string& line : rData["spawns"]) {
        int x = 0;
        for (char ch : line) {
            std::string symbol(1, ch);
            if (symbol == "." || symbol == " " || !legend.contains(symbol)) {
                x++; continue;
            }

            json spawnInfo = legend[symbol];
            std::string type = spawnInfo["type"];
            std::string templateID = spawnInfo["id"];

            std::cout << "[Spawn] Attempting to spawn " << type << " with template '" << templateID << "' at (" << x << "," << y << ") in room " << roomID << std::endl;

            json finalOverrides = json::object();

            if (floorSettings.contains("overrides") && floorSettings["overrides"].contains(type)) {
                for (auto& globalOver : floorSettings["overrides"][type]) {
                    if (globalOver["id"] == templateID) {
                        finalOverrides.update(globalOver);
                    }
                }
            }

            if (spawnInfo.contains("overrides")) {
                finalOverrides.update(spawnInfo["overrides"]);
            }

            if (type == "mob") {
                float respawnTime = spawnInfo.value("respawn_time", 30.0f);
                bool shouldRespawn = spawnInfo.value("respawn", true);

                if (shouldRespawn && ctx.respawnSystem) {
                    ctx.respawnSystem->CreateSpawnPoint(templateID, respawnTime, x, y, roomID);
                } else {
                    ctx.factories->mobs.CreateMob(templateID, finalOverrides, x, y, roomID);
                }
            }
            else if (type == "item") {
                ctx.factories->items.CreateItem(templateID,finalOverrides,x,y,roomID);
            }
            else if (type == "interactable") {
				ctx.factories->interactables.CreateInteractable(templateID, json::object(), x, y, roomID);
			}
            else if (type == "npc") {
                ctx.factories->mobs.CreateMob(templateID, finalOverrides, x, y, roomID);
            }
            x++;
        }
        y++;
    }
}

EntityID World::GetRoomEntity(int roomId) {
    if (!roomFactory) return 0;
    return roomFactory->GetRoomById(roomId);
}
