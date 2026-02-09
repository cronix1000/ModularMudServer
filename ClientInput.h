#pragma once
#include <string>

struct ClientInput {
    int clientID;
    std::string rawText; // This holds "password123", "kill goblin", or "yes"
};