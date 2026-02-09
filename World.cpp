#include "World.h"
#include <fstream>
#include <iostream>
#include "itemFactory.h"
#include "mobFactory.h"
#include "FactoryManager.h"
#include "GameContext.h"
#include "InteractableFactory.h"

const std::string REGION_PATH = "regions/";

#include <nlohmann/json.hpp> 
using json = nlohmann::json;
namespace fs = std::filesystem;
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
    return Direction::North; // Default/Error case
}
void World::LoadWorld(const std::string& filepath, GameContext& ctx) {
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
        int id = rData["id"];
        std::string name = rData["name"];
        std::string desc = rData["description"];
        int width = rData.value("width", 0);
        int height = rData.value("height", 0);

        // 1. Create the C++ Room Object
        Room* newRoom = new Room(id, name, desc);

        if (width > 0 && height > 0) {

            newRoom->InitializeGrid(width, height);

            if (rData.contains("layout")) {

                std::vector<std::string> layoutLines = rData["layout"];

                newRoom->LoadFromMap(layoutLines);

            }

        }

        roomMap[id] = newRoom;

        int roomEntity = ctx.registry->CreateEntity();

        ctx.registry->AddComponent<RoomComponent>(roomEntity, RoomComponent{ id, newRoom });
        

        if (rData.contains("scripts")) {
        ScriptComponent scripts = ScriptComponent();
            scripts.scripts_path.emplace("on_enter", rData["scripts"].value("on_enter", ""));
            scripts.scripts_path.emplace("on_exit", rData["scripts"].value("on_exit", ""));
            scripts.scripts_path.emplace("pulse", rData["scripts"].value("pulse", ""));
            ctx.registry->AddComponent<ScriptComponent>(roomEntity,
                scripts);
        }

        newRoom->SetEntityID(roomEntity);

        roomMap[id] = newRoom;
    }

    // --- PASS 2: LOAD EXITS ---
    for (const auto& rData : worldData["rooms"]) {
        int id = rData["id"];
        Room* currentRoom = roomMap[id];

        if (rData.contains("exits")) {
            for (auto& [dirString, exitValue] : rData["exits"].items()) {
                Direction dir = StringToDirection(dirString);

                // Handle Object format: "north": { "target_room": 2, ... }
                if (exitValue.is_object()) {
                    int targetID = exitValue["target_room"];
                    int destX = exitValue.value("dest_x", -1);
                    int destY = exitValue.value("dest_y", -1);
                    currentRoom->AddExit(dir, targetID, destX, destY);
                }
                // Handle simple format: "north": 2
                else if (exitValue.is_number_integer()) {
                    currentRoom->AddExit(dir, (int)exitValue);
                }
            }
        }
    }

    // --- PASS 3: LOAD LOCAL TILE LEGEND (Visual Overrides) ---
    for (const auto& rData : worldData["rooms"]) {
        int id = rData["id"];
        Room* currentRoom = roomMap[id];

        if (rData.contains("floor_legend")) {
            for (auto& [key, val] : rData["floor_legend"].items()) {
                if (key.empty()) continue;
                char symbol = key[0];

                TerrainDef terrain = TerrainDef{
                    symbol,
                    val.value("name", "Unknown Terrain"),
                        val.value("color", "white"),
                        val.value("blocks_move", false),
                        val.value("blocks_sight", false),
                        val.value("move_cost", 1)
                };
                currentRoom->AddLocalTerrain(symbol, terrain);
            }
        }
    }

    // --- PASS 4: SPAWNS (MOBS) - Handles both Grid and List styles ---
    for (const auto& rData : worldData["rooms"]) {
        int roomID = rData["id"];

        if (rData.contains("spawns")) {
            const auto& spawns = rData["spawns"];
            if (spawns.empty()) continue;

            // STYLE A: Grid Based (List of Strings like Room 1)
            if (spawns[0].is_string()) {
                // We need a legend to decode the grid
                if (rData.contains("spawn_legend")) {
                    json legend = rData["spawn_legend"];
                    int y = 0;
                    for (const std::string& line : spawns) {
                        int x = 0;
                        for (char symbol : line) {
                            if (symbol == ' ')
                                continue;
                            std::string symStr(1, symbol);
                            if (legend.contains(symStr)) {
                                // Found a mob symbol!
                                json spawnData = legend[symStr];
                                std::string ID = spawnData["id"];
                                std::string type = spawnData["type"];
                                json overrides = spawnData.value("overrides", json::object());

                                // Call your Factory here
                                // factory.Spawn(registry, mobID, x, y, roomID, overrides);
                                if (type == "mob") {
                                    ctx.factories->mobs.Spawn(ID, x, y, roomID, 3, true, overrides);
                                }
                                else if (type == "item") {
                                    ctx.factories->items.CreateItem(ID, overrides,x, y, roomID);
                                }
                            }
                            x++;
                        }
                        y++;
                    }
                }
            }
        }

        // --- PASS 5: INTERACTABLES (Chests, Levers) ---
        if (rData.contains("interactables")) {
            for (const auto& item : rData["interactables"]) {
                std::string type = item.value("type", "Unknown");
                int x = item.value("x", 0);
                int y = item.value("y", 0);
                json components = item.value("components", json::object());

                // factory.SpawnObject(registry, type, x, y, roomID, components);
            }
        }

        // --- PASS 5: SCRIPTS (on_enter, on_exit, pulse) ---
        if (rData.contains("scripts")) {
            int roomID = rData["id"];
            Room* currentRoom = roomMap[roomID]; // Assuming this is valid

            auto& sData = rData["scripts"];
            if (sData.contains("on_enter")) {
                currentRoom->script.on_enter = sData["on_enter"];
            }
            if (sData.contains("on_exit")) {
                currentRoom->script.on_exit = sData["on_exit"];
            }
        }
    }

    std::cout << "World Loaded Successfully (" << roomMap.size() << " rooms)." << std::endl;
}

bool World::CheckIfRegionLoaded(const std::string& regionPath)
{
    if (loadedRegions.find(regionPath) == loadedRegions.end()) {
        return false;
    }
    return true;
}

void World::LoadRegion(const std::string& region, GameContext& ctx)
{
    if (CheckIfRegionLoaded(region))
        return;

    std::string settingsPath = REGION_PATH + region + "/floor_settings.json";
    json floorSettings;
    std::ifstream sFile(settingsPath);
    if (sFile.is_open()) {
        sFile >> floorSettings;
    }

    // 2. Iterate through all .json files in the folder (C++17 filesystem)
    for (const auto& entry : fs::directory_iterator(REGION_PATH + region)) {
        if (entry.path().extension() != ".json" ||
            entry.path().filename() == "floor_settings.json") continue;

        LoadRoomFile(entry.path().string(), floorSettings, ctx);
    }
    
    loadedRegions.insert(region);
    
}

void World::LoadRoomFile(const std::string& path, const json& floorSettings, GameContext& ctx)
{
    std::ifstream file(path);
    json rData;
    file >> rData;

    int id = rData["id"];
    Room* newRoom = new Room(id, rData["name"], rData["description"]);
    newRoom->InitializeGrid(rData["width"], rData["height"]);

    if (rData.contains("layout")) {
        newRoom->LoadFromMap(rData["layout"]);
    }

    // Register Room
    roomMap[id] = newRoom;
    int roomEnt = ctx.registry->CreateEntity();
    ctx.registry->AddComponent<RoomComponent>(roomEnt, RoomComponent{ id, newRoom });

    // Pass 1: Exits
    if (rData.contains("exits")) {
        for (auto& [dirStr, exitVal] : rData["exits"].items()) {
            newRoom->AddExit(StringToDirection(dirStr),
                exitVal["target_room"],
                exitVal.value("dest_x", -1),
                exitVal.value("dest_y", -1));
        }
    }

    if (rData.contains("spawn")) {
        newRoom->spawn.first = rData["spawn"].value("x", 3);
        newRoom->spawn.second = rData["spawn"].value("y", 3);
    
    }

    // Pass 2: Spawns (Merging Floor Overrides + Room Overrides)
    if (rData.contains("spawns") && rData.contains("spawn_legend")) {
        ParseSpawns(rData, floorSettings, ctx);
    }

    // Scripts
    if (rData.contains("scripts")) {
        ScriptComponent scripts = ScriptComponent();
        scripts.scripts_path.emplace("on_enter", rData["scripts"].value("on_enter", ""));
        scripts.scripts_path.emplace("on_exit", rData["scripts"].value("on_exit", ""));
        scripts.scripts_path.emplace("pulse", rData["scripts"].value("pulse", ""));
        ctx.registry->AddComponent<ScriptComponent>(roomEnt,
            scripts);
    }


}

void World::ParseSpawns(const json& rData, const json& floorSettings, GameContext& ctx)
{
    int roomID = rData["id"];
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
                ctx.factories->mobs.Spawn(templateID, x, y, roomID, 3, true, finalOverrides);
            }
            else if (type == "item") {
                ctx.factories->items.CreateItem(templateID,finalOverrides,x,y,roomID);
            }
            else if (type == "interactable") {
                ctx.factories->interactables.CreateInteractable(templateID, x, y, roomID);
            }
            x++;
        }
        y++;
    }
}

Room* World::GetRoom(int id) {
    // 1. Try to find the iterator
    auto it = roomMap.find(id);

    if (it != roomMap.end()) {

        return it->second;
    }

    return nullptr;
}