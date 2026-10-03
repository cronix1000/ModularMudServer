#pragma once

#include "CommandTrie.h"

// Type definitions
using EntityID = int;

// Forward declarations
class ClientConnection;
struct GameContext;
class CommandRegistry;

class CombatCommandHandler {
public:
    static void RegisterAll(CommandRegistry& registry);

private:
    static CommandResult HandleAttack(ClientConnection* client,
                                      const std::vector<std::string>& params,
                                      GameContext& ctx);

    static CommandResult HandleCast(ClientConnection* client,
                                    const std::vector<std::string>& params,
                                    GameContext& ctx);

    static CommandResult HandleKill(ClientConnection* client,
                                    const std::vector<std::string>& params,
                                    GameContext& ctx);
};