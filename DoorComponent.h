#pragma once
#include <string>

struct DoorComponent {
    bool is_open = false;
    bool is_locked = false;
    std::string key_id = "";
    int destination_room = -1;
    int destination_x = -1;
    int destination_y = -1;
    std::string open_message = "The door creaks open.";
    std::string close_message = "The door slams shut.";
};