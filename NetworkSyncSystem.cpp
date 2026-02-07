#include "NetworkSyncSystem.h"
#include "Component.h"
#include <string>
#include <vector>
#include <sstream>
#include <regex>
#include "TextHelperFunctions.h"

// Telnet Constants
const char IAC = (char)255;
const char SB = (char)250;
const char SE = (char)240;
const char GMCP = (char)201;


NetworkSyncSystem::NetworkSyncSystem(GameContext& c) : ctx(c) {}


void NetworkSyncSystem::Run() {
    // Process entities whose position has changed.
    for (EntityID id : ctx.registry.view<PositionChangedComponent>()) {
        ClientComponent* client = ctx.registry.GetComponent<ClientComponent>(id);
        if (client) {
            SendLook(client->client);
        }
        // Remove the tag component now that it has been processed.
        ctx.registry.RemoveComponent<PositionChangedComponent>(id);
    }

    // Process entities that have just logged in.
    for (EntityID id : ctx.registry.view<PlayerLoginComponent>()) {
        // The original loop body was empty.
        // The primary goal seems to be clearing the component, which we do now.
        // If there was intended logic here, it would go before the remove call.
        ctx.registry.RemoveComponent<PlayerLoginComponent>(id);
    }
}

void NetworkSyncSystem::SendMapUpdate(ClientConnection* client)
{
    PositionComponent* pos = ctx.registry.GetComponent<PositionComponent>(client->playerEntityID);
    
    if (!pos) return;

    int radius = 2;
    std::vector<std::string> gridRows;

    //client->QueueMessage()
}

void NetworkSyncSystem::SendLook(ClientConnection* client)
{
    // 1. Get Data
    PositionComponent* playerPos = ctx.registry.GetComponent<PositionComponent>(client->playerEntityID);
    if (!playerPos) return;

    Room* room = ctx.worldManager.world->GetRoom(playerPos->roomId);
    if (!room) return;

    // Create an overlay grid to place entities on.
    std::vector<std::vector<VisualComponent*>> entityOverlay(
        room->GetHeight() + 1,
        std::vector<VisualComponent*>(room->GetWidth() + 1, nullptr)
    );

    // This is the new pattern for iterating entities with multiple components.
    // We get the views for both components and iterate the smaller one for efficiency.
    const auto& pos_entities = ctx.registry.view<PositionComponent>();
    const auto& vis_entities = ctx.registry.view<VisualComponent>();

    // The result of a ternary operator can be a temporary, so we can't store it in a non-const l-value reference.
    // Using `const auto&` for `smaller_view` is safe and efficient.
    const auto& smaller_view = (pos_entities.size() < vis_entities.size()) ? pos_entities : vis_entities;
    // `larger_view_type` is initialized from a temporary, so we make a copy.
    auto larger_view_type = (pos_entities.size() < vis_entities.size()) 
        ? std::type_index(typeid(VisualComponent)) 
        : std::type_index(typeid(PositionComponent));

    for (EntityID id : smaller_view) {
        // Check if the entity has the other component.
        bool has_other_component = (larger_view_type == std::type_index(typeid(VisualComponent)))
            ? ctx.registry.HasComponent<VisualComponent>(id)
            : ctx.registry.HasComponent<PositionComponent>(id);

        if (has_other_component) {
            PositionComponent* entPos = ctx.registry.GetComponent<PositionComponent>(id);
            VisualComponent* entVis = ctx.registry.GetComponent<VisualComponent>(id);

            // Only process if they are in the current room.
            if (entPos->roomId == room->GetId()) {
                if (entPos->x >= 0 && entPos->x <= room->GetWidth() && entPos->y >= 0 && entPos->y <= room->GetHeight()) {
                    entityOverlay[entPos->y][entPos->x] = entVis;
                }
            }
        }
    }
            
    std::stringstream text;
    std::vector<std::string> gridRows;
    std::string lastColor = "";

    // 3. Loop through the room tiles and draw them.
    for (int y = 0; y <= room->GetHeight(); y++) {
        std::string row = "";
        for (int x = 0; x <= room->GetWidth(); x++) {
            std::string currentColor;
            std::string symbol;

            // 1. Determine which symbol to use (entity or terrain).
            if (entityOverlay[y][x] != nullptr) {
                if (entityOverlay[y][x]->color != "")
                    currentColor = entityOverlay[y][x]->color;
                symbol = entityOverlay[y][x]->symbol;
            }
            else {
                const TerrainDef* type = room->GetTile(x, y);
                currentColor = type->color;
                symbol = type->symbol;
            }

            // 2. Handle Color Transitions to optimize output.
            if (currentColor != lastColor) {
                row += currentColor;
                lastColor = currentColor;
            }

            // 3. Add padding to ensure consistent tile width.
            row += symbol;
            int paddingNeeded = 3 - (int)symbol.length();
            for (int i = 0; i < paddingNeeded; i++) {
                row += ' ';
            }
        }
        row += "&w"; // Reset color at end of row.
        lastColor = "&w";
        gridRows.push_back(row);
    }

    text << "\r\n"; // Space between room description and map.
    for (const std::string& line : gridRows) {
        text << line << "\r\n";
    }
    
    client->QueueMessage(TextHelperFunctions::Colorize(text.str()));
}