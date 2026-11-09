#pragma once // Always include a header guard
#include <string>

struct GameContext; 
class ClientConnection;
using EntityID = int;

class PlayerFactory {
public: // Move public to the top or specifically for the constructor
    PlayerFactory(GameContext& g) : ctx(g) {}

    EntityID LoadPlayer(std::string accountName, ClientConnection* connection);

private:
    GameContext& ctx;
};