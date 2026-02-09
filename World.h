#pragma once
#include "Room.h"
#include "Direction.h"
#include <sstream>
#include "TerrainDef.h"
#include "Registry.h"
#include "RoomComponent.h"  
#include "ScriptComponent.h"
#include <nlohmann/json.hpp>
#include <set>


using json = nlohmann::json;

class ItemFactory;
class MobFactory;
struct GameContext;
class World
{
public:
	World();
	~World();
	void LoadWorld(const std::string& filepath, GameContext& ctx);
	bool CheckIfRegionLoaded(const std::string& regionPath);
	bool LoadRegion(const std::string& regionPath, GameContext& ctx);
	bool LoadRoomFile(const std::string& path, const json& floorSettings, GameContext& ctx);
	void ParseSpawns(const json& rData, const json& floorSettings, GameContext& ctx);
	Room* GetRoom(int id);
private:
	std::map<int, Room*> roomMap;
	std::set<std::string> loadedRegions;
};
