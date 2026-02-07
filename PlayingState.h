#pragma once
#include "GameState.h"
#include "GameEngine.h"
#include "ClientConnection.h"
#include "MoveIntentComponent.h"
#include "CommandInterpreter.h"
#include "DirtyFlagComponents.h"
#include "Registry.h"
#include "GameContext.h"
class PlayingState : public GameState
{
public:
    GameContext& ctx; // Reference to the "World"
    std::unique_ptr<Command> currentCmd;

public:
    PlayingState(GameContext& context)
        : ctx(context),
        currentCmd(std::make_unique<Command>())
    {
        // Everything is initialized and memory is managed!
    }

    ~PlayingState() {

    }

    void OnEnter(ClientConnection* client) override {
    }

    void HandleInput(ClientConnection* client, std::vector<std::string> p) override {

        currentCmd->ParseInput(p);

        currentCmd->ToLower();
        ctx.interpreter->Interpret(client, currentCmd.get());

    }
};