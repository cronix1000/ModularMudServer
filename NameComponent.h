#pragma once
#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <vector>

struct NameComponent {
    std::string displayName;
    std::vector<std::string> keywords;

    NameComponent() = default;

    NameComponent(const std::string& name) : displayName(name) {
        std::stringstream ss(displayName);
        std::string token;
        while (ss >> token) {
            std::transform(token.begin(), token.end(), token.begin(),
                [](unsigned char c) { return std::tolower(c); });

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

    bool MatchesPhrase(const std::vector<std::string>& queryTokens) const {
        if (queryTokens.empty()) return false;
        if (queryTokens.size() > keywords.size()) return false;

        size_t ki = 0;
        for (const auto& qt : queryTokens) {
            bool found = false;
            while (ki < keywords.size()) {
                if (keywords[ki].find(qt) != std::string::npos) {
                    found = true;
                    ki++;
                    break;
                }
                ki++;
            }
            if (!found) return false;
        }
        return true;
    }
};