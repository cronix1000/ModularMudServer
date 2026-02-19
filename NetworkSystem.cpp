#include "NetworkSystem.h"
#include "ClientConnection.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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

        Room* room = ctx.worldManager->world->GetRoom(data.RoomID);
        ClientComponent* client = ctx.registry->GetComponent<ClientComponent>(data.EntityID);

        if (client && client->client && room) {
            // Build JSON data as string
            json jsonData = {
                {"room_id", data.RoomID},
                {"room_name", room->Name},
                {"description", room->Description}
            };
            
            GameMessage msg;
            msg.type = "room_enter";
            msg.consoleText = "&w" + room->Name + "&w\r\n" + room->Description + "\r\n";
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
