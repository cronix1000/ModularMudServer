#pragma once
#include <string>
struct PortalComponent {
    int destination_room;
    std::string direction_command; // e.g., "north", "enter rift", "climb"
    bool is_open;
    bool is_locked;
    int key_id;
};