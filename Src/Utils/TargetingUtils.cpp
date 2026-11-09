#include "TargetingUtils.h"

std::pair<std::vector<std::string>, int> TargetingUtils::ParseTarget(const std::vector<std::string>& params) {
    if (params.empty()) return { {}, 0 };
    const std::string& last = params.back();
    bool isNumber = !last.empty() && std::all_of(last.begin(), last.end(), ::isdigit);
    if (isNumber && params.size() > 1) {
        int idx = std::stoi(last);
        std::vector<std::string> tokens(params.begin(), params.end() - 1);
        return { tokens, idx };
    }
    return { params, 0 };
}

std::string TargetingUtils::TokensToString(const std::vector<std::string>& tokens) {
    std::string out;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i) out += ' ';
        out += tokens[i];
    }
    return out;
}