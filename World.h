#pragma once

#include "Direction.h"
#include "Registry.h"
#include "RoomComponents.h"
#include "ScriptComponent.h"
#include "TerrainDef.h"

#include <nlohmann/json.hpp>
#include <set>
#include <sstream>

using json = nlohmann::json;

class ItemFactory;
class MobFactory;
class RoomFactory;
struct GameContext;
class SQLiteDatabase;

class World {
public:
	World();
	~World();
	bool CheckIfRegionLoaded(const std::string& regionId);
	bool LoadRegion(const std::string& regionId, GameContext& ctx);
	bool LoadRoomFile(const std::string& path, const json& floorSettings, GameContext& ctx);
	bool LoadRoomFromJson(const json& rData, const json& floorSettings, GameContext& ctx);
	void ParseSpawns(const json& rData, int roomID, const json& floorSettings, GameContext& ctx);

	// Deprecated: Use WorldManager::GetRoomLayout/GetRoomExits instead
	// Kept temporarily for backward compatibility during migration
	EntityID GetRoomEntity(int roomId);

private:
	std::set<std::string> loadedRegions;
	RoomFactory* roomFactory = nullptr;
};