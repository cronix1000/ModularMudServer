#pragma once
#include <string>
#include "Command.h"
class ClientConnection; // Forward declaration

class GameState {
public:
    virtual ~GameState() {}

    // push a menu to the user
    virtual void OnEnter(ClientConnection* client) = 0;

    virtual void OnResume(ClientConnection* client) {
        OnEnter(client);
    }

    // handle progress & input in the menu and
    virtual void HandleInput(ClientConnection* client, std::vector<std::string> input) = 0;

    // Receive a parsed GMCP packet from the client. Default is a no-op;
    // states that care about GMCP (e.g. PlayingState) override this.
    virtual void HandleGMCP(ClientConnection* client, const std::string& module, const std::string& json) {
        (void)client; (void)module; (void)json;
    }
};