#include "World.h"
#include <iostream>
#include "ItemFactory.h"
#include "MobFactory.h"
#include "FactoryManager.h"
#include "GameContext.h"
#include "InteractableFactory.h"
#include "RespawnSystem.h"
#include "RoomFactory.h"
#include "Registry.h"

const std::string DEFAULT_WORLD_ID = "default";

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

    std::cerr << "World::LoadRegion: region '" << regionId << "' not found in database. "
              << "The legacy on-disk regions/<id>/*.json fallback has been removed; "
              << "create the region, rooms, and spawns in MudAdmin instead." << std::endl;
    return false;
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
