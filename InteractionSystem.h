#pragma once
#include "InteractableContext.h"
#include "RoomComponents.h"

struct GameContext;
class Registry;

RoomRulesComponent* FindRoomRulesComponent(Registry& registry, int roomId);

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