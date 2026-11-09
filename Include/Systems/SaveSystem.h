#pragma once
#include <vector>
using EntityID = int;
struct GameContext;
class Registry;

class SaveSystem
{
	float saveTimer = 0.0f;
	const float SAVE_INTERVAL = 60.0f;
public:
	GameContext& ctx;
	SaveSystem(GameContext& g) : ctx(g) {};
	~SaveSystem();
	std::vector<EntityID> dirty;
	void Run(float deltaTime);
	void SaveNow(EntityID client);
	void SaveAllPlayers();
	void SaveDirtyEntities();
};