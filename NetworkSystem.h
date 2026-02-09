#pragma once
#include "Registry.h"
#include "EventBus.h"
#include "ClientComponent.h"
#include "PositionComponent.h"
#include "Room.h"
#include "World.h"
#include "GameContext.h"
#include "TextHelperFunctions.h"

class NetworkSystem {
	GameContext& ctx;
public:
	NetworkSystem(GameContext& gc) : ctx(gc){};
	void SetupListeners() 
	{
		ctx.eventBus->Subscribe(EventType::RoomEntered, [this](const EventContext& ectx) {
			if (!std::holds_alternative<RoomEventData>(ectx.data)) return;
			const auto& data = std::get<RoomEventData>(ectx.data);

			Room* room = ctx.worldManager->world->GetRoom(data.RoomID);
			ClientComponent* client = ctx.registry->GetComponent<ClientComponent>(data.EntityID);

          
            if (client && client->client && room) {
                std::string output = "&w" + room->Name + "&w\r\n" + room->Description + "\r\n";
                client->client->QueueMessage(TextHelperFunctions::Colorize(output));
            }
		});
	}
};