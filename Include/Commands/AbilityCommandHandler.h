#pragma once
#include "CommandTrie.h"

// Type definitions
using EntityID = int;

// Forward declarations
class ClientConnection;
struct GameContext;
class CommandRegistry;

class AbilityCommandHandler {
public:
    static void RegisterAll(CommandRegistry& registry);

private:
    static CommandResult HandleAbilityCall(ClientConnection* client,
        const std::vector<std::string>& params,
        GameContext& ctx);

    static CommandResult HandleCastCall(ClientConnection* client,
        const std::vector<std::string>& params,
        GameContext& ctx);
};