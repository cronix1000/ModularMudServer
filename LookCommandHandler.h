#pragma once

#include "CommandTrie.h"
#include <vector>
#include <string>

class ClientConnection;
struct GameContext;
class CommandRegistry;

class LookCommandHandler {
public:
    static void RegisterAll(CommandRegistry& registry);

private:
    static CommandResult HandleLook(ClientConnection* client,
                                    const std::vector<std::string>& params,
                                    GameContext& ctx);
};