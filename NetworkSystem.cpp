#include "NetworkSystem.h"
#include "ClientConnection.h"
#include "GMCPModules.h"
#include "WorldManager.h"
#include "RoomComponents.h"
#include "Registry.h"
#include "CommandRegistry.h"

void NetworkSystem::SetupListeners()
{
	ctx.eventBus->Subscribe(EventType::RoomEntered, [this](const EventContext& ectx) {
		if (!std::holds_alternative<RoomEventData>(ectx.data)) return;
		const auto& data = std::get<RoomEventData>(ectx.data);

		const RoomIdentityComponent* roomIdentity = nullptr;
		for (EntityID roomEnt : ctx.registry->view<RoomIdentityComponent>()) {
			auto* identity = ctx.registry->GetComponent<RoomIdentityComponent>(roomEnt);
			if (identity && identity->roomId == data.RoomID) {
				roomIdentity = identity;
				break;
			}
		}

		ClientComponent* client = ctx.registry->GetComponent<ClientComponent>(data.EntityID);

		if (client && client->client && roomIdentity) {
			json roomInfo = {
				{"num",   data.RoomID},
				{"name",  roomIdentity->name},
				{"desc",  roomIdentity->description},
				{"terrain", "city"}
			};

			GameMessage msg;
			msg.type = gmcp::Room_Info;
			msg.consoleText = "&w" + roomIdentity->name + "&w\r\n" + roomIdentity->description + "\r\n";
			msg.jsonData = roomInfo.dump();
			client->QueueGameMessage(msg);
		}
	});

	ctx.eventBus->Subscribe(EventType::PlayerJoined, [this](const EventContext& ectx) {
		if (!std::holds_alternative<PlayerLoggedInData>(ectx.data)) return;
		const auto& data = std::get<PlayerLoggedInData>(ectx.data);

		ClientComponent* client = ctx.registry->GetComponent<ClientComponent>(data.playerID);

		if (client) {
			GameMessage msg;
			msg.type = gmcp::Command_List;
			PermissionLevel level = static_cast<PermissionLevel>(data.permissionLevel);
			msg.jsonData = ctx.commandRegistry->GetCommandListJson(level).dump();
			client->QueueGameMessage(msg);
		}
	});
}

void NetworkSystem::FlushQueues()
{
	for (EntityID entityID : ctx.registry->view<ClientComponent>()) {
		ClientComponent* clientComp = ctx.registry->GetComponent<ClientComponent>(entityID);
		if (!clientComp || !clientComp->client) continue;

		if (!clientComp->HasPendingMessages()) continue;

		for (const GameMessage& msg : clientComp->messageQueue) {
			if (clientComp->isWebClient) {
				SendToWebClient(clientComp->client, clientComp, msg);
			} else {
				SendToTerminalClient(clientComp->client, clientComp, msg);
			}
		}

		clientComp->ClearMessageQueue();
	}
}

void NetworkSystem::SendCommandList(EntityID playerId)
{
	ClientComponent* clientComp = ctx.registry->GetComponent<ClientComponent>(playerId);
	if (!clientComp || !clientComp->client) return;

	PermissionLevel playerPerm = PermissionLevel::Guest;
	if (ctx.commandRegistry) {
		playerPerm = ctx.commandRegistry->GetPlayerPermission(playerId);
	}

	json cmdList = BuildCommandListJson(playerPerm);

	GameMessage msg;
	msg.type = gmcp::Command_List;
	msg.consoleText = "[Available commands loaded]\r\n";
	msg.jsonData = cmdList.dump();

	clientComp->QueueGameMessage(msg);
}

json NetworkSystem::BuildCommandListJson(PermissionLevel playerPerm)
{
	json result = json::array();

	if (!ctx.commandRegistry) return result;

	result = ctx.commandRegistry->GetCommandListJson(playerPerm);

	return result;
}

void NetworkSystem::SendToWebClient(ClientConnection* client,
                                    ClientComponent* clientComp,
                                    const GameMessage& msg)
{
	(void)clientComp;

	if (!msg.jsonData.empty() && msg.jsonData != "{}") {
		if (clientComp == nullptr || !client->isSubscribed(msg.type)) return;
		std::string envelope = BuildWebEnvelope(msg.type, msg.consoleText, msg.jsonData);
		client->QueueMessage(envelope);
	}
	else if (!msg.consoleText.empty()) {
		json envelope = {
			{"channel", "text"},
			{"data",    msg.consoleText}
		};
		client->QueueMessage(envelope.dump() + "\n");
	}
}

void NetworkSystem::SendToTerminalClient(ClientConnection* client,
                                        ClientComponent* clientComp,
                                        const GameMessage& msg)
{
	if (!msg.consoleText.empty()) {
		client->QueueMessage(TextHelperFunctions::Colorize(msg.consoleText));
	}

	if (msg.jsonData.empty() || msg.jsonData == "{}") return;

	if (clientComp == nullptr) return;
	if (!clientComp->hasGMCP) return;
	if (!client->isSubscribed(msg.type)) return;

	std::string gmcpPacket = BuildGMCPFrame(msg.type, msg.jsonData);
	client->SendPacket(gmcpPacket);
}

std::string NetworkSystem::BuildWebEnvelope(const std::string& module,
                                            const std::string& consoleText,
                                            const std::string& jsonDataStr)
{
	json envelope;
	envelope["channel"] = "gmcp";
	envelope["module"]  = module;

	if (!jsonDataStr.empty()) {
		try {
			envelope["data"] = json::parse(jsonDataStr);
		} catch (...) {
			envelope["data"] = json::object();
		}
	} else {
		envelope["data"] = json::object();
	}

	if (!consoleText.empty()) {
		envelope["text"] = consoleText;
	}

	return envelope.dump() + "\n";
}

std::string NetworkSystem::BuildGMCPFrame(const std::string& module, const std::string& jsonDataStr)
{
	return telnet::buildGMCPSubneg(module, jsonDataStr);
}
