#pragma once

#include "CommandTrie.h"

// Type definitions
using EntityID = int;

// Forward declarations
class ClientConnection;
struct GameContext;
class CommandRegistry;

class ItemCommandHandler {
public:
    static void RegisterAll(CommandRegistry& registry);

private:
    static CommandResult HandlePickup(ClientConnection* client,
                                      const std::vector<std::string>& params,
                                      GameContext& ctx);

    static CommandResult HandleDrop(ClientConnection* client,
                                    const std::vector<std::string>& params,
                                    GameContext& ctx);

    static CommandResult HandleEquip(ClientConnection* client,
                                     const std::vector<std::string>& params,
                                     GameContext& ctx);

    static CommandResult HandleInventory(ClientConnection* client,
                                         const std::vector<std::string>& params,
                                         GameContext& ctx);
};