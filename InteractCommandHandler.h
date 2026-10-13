#pragma once
#include "CommandTrie.h"

// Type definitions
using EntityID = int;

// Forward declarations
class ClientConnection;
struct GameContext;
struct PositionComponent;
class CommandRegistry;


class InteractCommandHandler {
public:
    static void RegisterAll(CommandRegistry& registry);

private:
    static CommandResult HandleInteract(ClientConnection* client,
        const std::vector<std::string>& params,
        GameContext& ctx);
    static CommandResult HandleOpen(ClientConnection* client,
        const std::vector<std::string>& params,
        GameContext& ctx);
    static CommandResult HandlePray(ClientConnection* client,
        const std::vector<std::string>& params,
        GameContext& ctx);
    static CommandResult ResolveAndIntent(ClientConnection* client,
        GameContext& ctx,
        EntityID playerID,
        PositionComponent* playerPos,
        const std::vector<std::string>& params,
        const std::string& action);
};
