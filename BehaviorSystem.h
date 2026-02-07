#pragma once
struct GameContext;
struct EventContext;

class BehaviorSystem
{
public:
	GameContext& ctx;
	BehaviorSystem(GameContext& g) : ctx(g) {}
	~BehaviorSystem() = default;
	void SetupListeners();
	void OnEntityDamaged(const EventContext& ectx);
};