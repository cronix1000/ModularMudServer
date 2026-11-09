#pragma once
#include <vector>
struct SocketComponent {
    int maxSockets = 0;
    std::vector<int> socketedGemIDs; // EntityIDs of gems inside

    // Simple constructor to reserve space
    SocketComponent(int max = 0) : maxSockets(max) {
        socketedGemIDs.resize(max, -1); // -1 means empty
    }
};