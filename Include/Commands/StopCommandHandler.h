#pragma once
#include "CommandRegistry.h"
#include "GameContext.h"
#include "ClientConnection.h"

class StopCommandHandler {
public:
    static void RegisterAll(CommandRegistry& registry);
    
    static CommandResult HandleStop(ClientConnection* client,
                                   const std::vector<std::string>& params,
                                   GameContext& ctx);
};