#pragma once
class GameContext;


using EntityID = int;  // Add this if not already defined

class MovementSystem
{
	GameContext& ctx;
public:
	MovementSystem(GameContext& c);
	~MovementSystem();
	void SetupListeners();
	void MovementSystemRun();

private:

};
