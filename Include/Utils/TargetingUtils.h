#pragma once
#include <utility>
#include <vector>
#include <string>
#include <algorithm>

class TargetingUtils {
public:
    static std::pair<std::vector<std::string>, int> ParseTarget(const std::vector<std::string>& params);

    static std::string TokensToString(const std::vector<std::string>& tokens);
};