#pragma once
class Room;
struct RoomComponent {
    int roomID;       // The integer ID from your JSON
    Room* roomPtr;    // A direct pointer for high-speed access
};