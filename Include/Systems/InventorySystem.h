#pragma once
#include "GameContext.h"

class InventorySystem
{
public:
	GameContext& ctx;
	
	InventorySystem(GameContext& g) : ctx(g) {};
	~InventorySystem() = default;

	void Run(float dt);

	void RecalculateStats(int entityID);

private:

};