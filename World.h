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

class World {
public:
	World();
	~World();
	void LoadWorld(const std::string& filepath, GameContext& ctx);
	bool CheckIfRegionLoaded(const std::string& regionPath);
	bool LoadRegion(const std::string& regionPath, GameContext& ctx);
	bool LoadRoomFile(const std::string& path, const json& floorSettings, GameContext& ctx);
	void ParseSpawns(const json& rData, const json& floorSettings, GameContext& ctx);
	
	// Deprecated: Use WorldManager::GetRoomLayout/GetRoomExits instead
	// Kept temporarily for backward compatibility during migration
	EntityID GetRoomEntity(int roomId);

private:
	std::set<std::string> loadedRegions;
	RoomFactory* roomFactory = nullptr;
};