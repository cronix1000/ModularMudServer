#pragma once

struct GameContext;

class CleanUpSystem
{
public:
	GameContext& ctx;
	CleanUpSystem(GameContext& g) : ctx(g) {}
	~CleanUpSystem() = default;
	void run();

};