#pragma once
#include <string>
#include <map>

// Generic interactable component for custom scripted interactions
struct InteractableComponent {
    std::string interaction_type = "generic";
    std::map<std::string, std::string> custom_data; // Key-value pairs for script use
    std::map<std::string, int> int_data; // Integer values
    std::map<std::string, float> float_data; // Float values  
    std::map<std::string, bool> bool_data; // Boolean values
    int cooldown_seconds = 0;
    float last_used_time = 0.0f;
    int uses_remaining = -1; // -1 for infinite uses
};