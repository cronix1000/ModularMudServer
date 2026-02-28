#include "NetworkSystem.h"
#include "ClientConnection.h"
#include "WorldManager.h"
#include "RoomComponents.h"
#include "Registry.h"

// Telnet Constants for GMCP
const char IAC = static_cast<char>(255);
const char SB = static_cast<char>(250);
const char SE = static_cast<char>(240);
const char GMCP = static_cast<char>(201);

void NetworkSystem::SetupListeners() 
{
    ctx.eventBus->Subscribe(EventType::RoomEntered, [this](const EventContext& ectx) {
        if (!std::holds_alternative<RoomEventData>(ectx.data)) return;
        const auto& data = std::get<RoomEventData>(ectx.data);

        // Get room identity component for room name/description
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
            // Build JSON data as string
            json jsonData = {
                {"room_id", data.RoomID},
                {"room_name", roomIdentity->name},
                {"description", roomIdentity->description}
            };
            
            GameMessage msg;
            msg.type = "room_enter";
            msg.consoleText = "&w" + roomIdentity->name + "&w\r\n" + roomIdentity->description + "\r\n";
            msg.jsonData = jsonData.dump();
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
                SendToWebClient(clientComp->client, msg);
            } else {
                SendToTerminalClient(clientComp->client, msg, clientComp->hasSideBar);
            }
        }
        
        clientComp->ClearMessageQueue();
    }
}

void NetworkSystem::SendToWebClient(ClientConnection* client, const GameMessage& msg)
{
    std::string jsonEnvelope = BuildJSONEnvelope(msg);
    client->QueueMessage(jsonEnvelope);
}

void NetworkSystem::SendToTerminalClient(ClientConnection* client, const GameMessage& msg, bool hasSideBar)
{
    // Always send the console text (with ANSI color parsing)
    client->QueueMessage(TextHelperFunctions::Colorize(msg.consoleText));
    
    // Optionally send GMCP data for clients that support it (e.g., Mudlet)
    if (hasSideBar && !msg.jsonData.empty() && msg.jsonData != "{}") {
        std::string gmcpPacket = BuildGMCPSession(msg.type, msg.jsonData);
        client->SendPacket(gmcpPacket);
    }
}

std::string NetworkSystem::BuildJSONEnvelope(const GameMessage& msg)
{
    json envelope;
    envelope["type"] = msg.type;
    envelope["console_text"] = msg.consoleText;
    
    // Parse the stored JSON string back into a json object
    if (!msg.jsonData.empty()) {
        try {
            envelope["ui_data"] = json::parse(msg.jsonData);
        } catch (...) {
            envelope["ui_data"] = json::object();
        }
    } else {
        envelope["ui_data"] = json::object();
    }
    
    return envelope.dump() + "\n";
}

std::string NetworkSystem::BuildGMCPSession(const std::string& module, const std::string& jsonDataStr)
{
    std::stringstream packet;
    
    // Build GMCP packet: IAC SB GMCP <module> <data> IAC SE
    packet << IAC << SB << GMCP;
    packet << "GameMessages." << module << " ";
    packet << jsonDataStr;
    packet << IAC << SE;
    
    return packet.str();
}
