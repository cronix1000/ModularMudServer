#pragma once
#include "WorldManager.h"
#include "Registry.h"
#include "MoveIntentComponent.h"  
#include "PositionComponent.h"    
#include <vector>                 
#include "EventBus.h"

#include "GameContext.h"
#include "ScriptManager.h"
#include "TextHelperFunctions.h"


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
