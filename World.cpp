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

namespace fs = std::filesystem;

const fs::path REGION_DIR = "regions";

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
    // load global terrains
    std::string globalTerrainFilePath = "global_terrain.json";
    std::ifstream file(globalTerrainFilePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open terrain file: " << globalTerrainFilePath << std::endl;
        return;
    }


    json terrainData;
    try {
        file >> terrainData;
    }
    catch (json::parse_error& e) {
        std::cerr << "JSON Parse Error: " << e.what() << std::endl;
        return;
    }

    try {

        if (terrainData.is_object()) {
            for (auto& [key, val] : terrainData.items()) {
                if (key.empty()) continue;
                char symbol = key[0];

                globalTerrain[symbol] = {
                    symbol,
                    // Use .value() for EVERYTHING to prevent crashes on typos
                    val.value("name", "Unknown Terrain"),
                    val.value("color", "white"),
                    val.value("blocks_move", false),
                    val.value("blocks_sight", false),
                    val.value("move_cost", 1)
                };
            }
        }

        printf("terrain loaded");
    }
    catch (const json::exception& e) {
        // This catches Parse errors AND Type errors (missing keys)
        std::cerr << "JSON Error in " << globalTerrainFilePath << ": " << e.what() << std::endl;
    }
    
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
    return Direction::North; // Default/Error case
}

void World::LoadWorld(const std::string& filepath, GameContext& ctx) {
    // Create room factory if not exists
    if (!roomFactory) {
        roomFactory = new RoomFactory(ctx);
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open world file: " << filepath << std::endl;
        return;
    }

    // 1. Read file safely into string buffer first (prevents empty input error)
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string fileContent = buffer.str();

    json worldData;
    try {
        worldData = json::parse(fileContent);
    }
    catch (json::parse_error& e) {
        std::cerr << "JSON Parse Error: " << e.what() << std::endl;
        return;
    }

    // --- PASS 1: CREATE ROOMS AND LAYOUTS ---
    for (const auto& rData : worldData["rooms"]) {
        // Use RoomFactory to create room entity with all components
        roomFactory->CreateRoom(rData);
    }

    std::cout << "World Loaded Successfully." << std::endl;
}

bool World::CheckIfRegionLoaded(const std::string& regionPath)
{
    if (loadedRegions.find(regionPath) == loadedRegions.end()) {
        return false;
    }
    return true;
}

bool World::LoadRegion(const std::string& region, GameContext& ctx)
{
    if (CheckIfRegionLoaded(region))
        return true;

    // Create room factory if not exists
    if (!roomFactory) {
        roomFactory = new RoomFactory(ctx);
    }

    fs::path regionDir;
    if (!ResolveRegionDirectory(region, regionDir)) {
        std::cerr << "World::LoadRegion: cannot find region '" << region << "' near "
            << fs::current_path() << std::endl;
        return false;
    }

    json floorSettings;
    fs::path settingsPath = regionDir / "floor_settings.json";
    if (fs::exists(settingsPath)) {
        std::ifstream sFile(settingsPath);
        try {
            sFile >> floorSettings;
        }
        catch (const json::parse_error& e) {
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
    
    loadedRegions.insert(region);
    return true;
}

bool World::LoadRoomFile(const std::string& path, const json& floorSettings, GameContext& ctx)
{
    // Create room factory if not exists
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

    int id = rData.value("id", -1);
    if (id < 0) {
        std::cerr << "World::LoadRoomFile: missing valid id in " << path << std::endl;
        return false;
    }

    // Use RoomFactory to create room
    EntityID roomEntity = roomFactory->CreateRoom(rData);
    if (roomEntity == 0) {
        std::cerr << "World::LoadRoomFile: Failed to create room from " << path << std::endl;
        return false;
    }

    // Handle spawns
    if (rData.contains("spawns") && rData.contains("spawn_legend")) {
        int logicalRoomId = rData.value("id", roomEntity);
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
        std::stringstream ss(line);
        std::string symbol;

        while (ss >> symbol) {
            if (symbol == "." || !legend.contains(symbol)) {
                x++; continue;
            }

            json spawnInfo = legend[symbol];
            std::string type = spawnInfo["type"];
            std::string templateID = spawnInfo["id"];
            
            std::cout << "[Spawn] Attempting to spawn " << type << " with template '" << templateID << "' at (" << x << "," << y << ") in room " << roomID << std::endl;

            // --- THE OVERRIDE MERGE ---
            json finalOverrides = json::object();

            // 1. Check Floor Overrides (e.g., Global floor health buff)
            if (floorSettings.contains("overrides") && floorSettings["overrides"].contains(type)) {
                for (auto& globalOver : floorSettings["overrides"][type]) {
                    if (globalOver["id"] == templateID) {
                        finalOverrides.update(globalOver);
                    }
                }
            }

            // 2. Apply Local Room Overrides (e.g., This specific goblin is weak)
            if (spawnInfo.contains("overrides")) {
                finalOverrides.update(spawnInfo["overrides"]);
            }

            // 3. Execution
            if (type == "mob") {
                // Check if this mob should have a spawn point (respawn capability)
                float respawnTime = spawnInfo.value("respawn_time", 30.0f); // Default 30 seconds
                bool shouldRespawn = spawnInfo.value("respawn", true); // Default true
                
                if (shouldRespawn && ctx.respawnSystem) {
                    // Create a spawn point that will manage this mob
                    ctx.respawnSystem->CreateSpawnPoint(templateID, respawnTime, x, y, roomID);
                } else {
                    // Just create the mob directly without respawn capability
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
                // NPCs are essentially mobs without respawn
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