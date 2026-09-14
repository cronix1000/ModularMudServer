#pragma once
#include "GameState.h"
#include "GameEngine.h"
#include "ClientConnection.h"
#include "MoveIntentComponent.h"
#include "CommandInterpreter.h"
#include "CommandRegistry.h"
#include "DirtyFlagComponents.h"
#include "Registry.h"
#include "GameContext.h"
#include "CommandChain.h"
#include <nlohmann/json.hpp>

class PlayingState : public GameState
{
public:
    GameContext& ctx; // Reference to the "World"

public:
    PlayingState(GameContext& context)
        : ctx(context)
    {
    }

    ~PlayingState() = default;

    void OnEnter(ClientConnection* client) override {
        // Commands are sent during login (in LoginState)
    }

    void HandleInput(ClientConnection* client, std::vector<std::string> p) override {
        // Reconstruct input from vector
        std::string input;
        for (size_t i = 0; i < p.size(); ++i) {
            if (i > 0) input += " ";
            input += p[i];
        }

        // Use new command registry with chain support
        if (ctx.commandRegistry) {
            ctx.commandRegistry->Execute(client, input);
        }
    }

    void HandleGMCP(ClientConnection* client, const std::string& module, const std::string& jsonText) override {
        if (module == "Client.Subscriptions.List") {
            try {
                auto j = nlohmann::json::parse(jsonText);
                if (j.is_array()) {
                    client->subscribedModules.clear();
                    for (const auto& v : j) {
                        if (v.is_string()) client->subscribedModules.insert(v.get<std::string>());
                    }
                }
            } catch (const nlohmann::json::exception&) {
                // Bad JSON; ignore.
            }
        }
        else if (module == "Core.Goodbye") {
            client->DisconnectGracefully();
        }
        else if (module == "Core.Hello" || module == "Core.Supports.Set") {
            // Informational; no action required.
        }
        // All other modules are server-initiated; clients don't send them.
    }
};