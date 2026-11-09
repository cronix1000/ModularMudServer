#pragma once
#include "GameContext.h"

class UpdateSystem
{
	GameContext& ctx;
	float timeElapsed = 0;
public:
	UpdateSystem(GameContext& ctx);
	~UpdateSystem();
	void Run();
	void Update(float dt);

private:

};
