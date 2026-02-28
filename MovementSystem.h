#pragma once

class GameContext;

using EntityID = int;

class MovementSystem {
	GameContext& ctx;

public:
	MovementSystem(GameContext& c);
	~MovementSystem();
	void SetupListeners();
	void MovementSystemRun();

private:
};
