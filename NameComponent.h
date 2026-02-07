#pragma once
#include <string>
#include <sstream>
#include <algorithm> 
#include <cctype>

struct NameComponent {
    std::string displayName;
    std::vector<std::string> keywords;

    // Default constructor (required for some ECS setups)
    NameComponent() = default;

    // Custom constructor that triggers the logic automatically
    NameComponent(const std::string& name) : displayName(name) {
        std::stringstream ss(displayName);
        std::string token;
        while (ss >> token) {
            // Lowercase the token
            std::transform(token.begin(), token.end(), token.begin(),
                [](unsigned char c) { return std::tolower(c); });

            // Filter out fluff words
            if (token == "a" || token == "the" || token == "of" || token == "an")
                continue;

            keywords.push_back(token);
        }
    }

    bool Matches(std::string input) const {
        std::transform(input.begin(), input.end(), input.begin(),
            [](unsigned char c) { return std::tolower(c); });

        for (const auto& k : keywords) {
            if (k == input) return true;
        }
        return false;
    }
};