#include "MessageSystem.h"
#include "EventBus.h"
#include "GameContext.h"
#include "Registry.h"
#include "ClientComponent.h"
#include "ItemComponent.h"
#include "NameComponent.h"
#include "TextHelperFunctions.h"


void MessageSystem::SanitizeMessage(std::string& message)
{

}

void MessageSystem::SubscribeToEvents()
{
	ctx.eventBus->Subscribe(EventType::ItemEquipped, [this](const EventContext& ectx) {
		if (!std::holds_alternative<ItemEquippedEventData>(ectx.data)) return;
		const auto& data = std::get<ItemEquippedEventData>(ectx.data);

	
		NameComponent* item = ctx.registry->GetComponent<NameComponent>(data.itemId);
		std::string display = "You Equipped " + item->displayName + "\n";
		ToPlayer(data.player, display);
	});
}

void MessageSystem::ToPlayer(int entityID, std::string msg)
{
	ClientComponent* client = ctx.registry->GetComponent<ClientComponent>(entityID);

	if (client) {
		client->client->QueueMessage(TextHelperFunctions::Colorize(msg));
	}

}

void MessageSystem::ToRoom(int roomID, std::string msg, int excludeID)
{
}

void MessageSystem::ToGlobal(std::string msg)
{

}
