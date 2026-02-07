#pragma once
#include "InteractableContext.h"

class GameContext;
class Registry;

class InteractionSystem
{
public:
	GameContext& ctx;
	InteractionSystem(GameContext& g) : ctx(g) {}
	~InteractionSystem();
	void run();

private:
	void HandleInteractableResult(int userEntityID, const InteractableResult& result);
};