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
};