#pragma once
struct TimeData {
    double globalTime = 0.0; // Total seconds since server start (Use double!)
    float deltaTime = 0.0f;  // Time passed since last frame (float is fine here)
};