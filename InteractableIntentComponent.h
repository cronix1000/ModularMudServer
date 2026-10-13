#pragma once
#include <string>

struct InteractableIntentComponent {
    int interactEntityId;
    std::string action; // "use", "activate", "open", "close", etc.
    std::string parameters; // Optional parameters for the interaction
};