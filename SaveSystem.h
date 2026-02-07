#pragma once


class GameContext;
class Registry;

class SaveSystem
{
	float saveTimer = 0.0f;
	const float SAVE_INTERVAL = 60.0f;
public:
	GameContext& ctx;
	SaveSystem(GameContext& g) : ctx(g) {};
	~SaveSystem();

	void Run(float deltaTime);
	void SaveDirtyEntities();
};