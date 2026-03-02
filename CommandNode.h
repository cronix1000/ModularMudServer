#pragma once
#include <unordered_map>
#include <string>
#include <memory>
struct CommandNode {
    std::unordered_map<std::string, std::unique_ptr<CommandNode>> children;
    std::string fullCommandPath;     // "eat summer sausage"
    CommandHandler* handler;          // nullptr if intermediate node
    PermissionLevel minPermission;    // 0 = all, 100 = admin, etc.
    std::vector<std::string> aliases; // For help/documentation
    bool isLua;                       // C++ vs Lua handler
};

enum class PermissionLevel : uint8_t {
    Guest = 0,
    Player = 10,
    Moderator = 50,
    Admin = 100
};

struct CommandPermissions {
    PermissionLevel minLevel;
    std::vector<std::string> requiredRoles;  // "wizard", "builder", etc.
    bool requireTarget;                      // Some commands need target
};