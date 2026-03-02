#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>

// Forward declarations
class ClientConnection;
struct GameContext;

enum class PermissionLevel : uint8_t {
	Guest = 0,
	Player = 10,
	Moderator = 50,
	Admin = 100
};

struct CommandResult {
	bool success;
	std::string message;
	
	CommandResult() : success(false) {}
	CommandResult(bool s) : success(s) {}
	CommandResult(bool s, const std::string& m) : success(s), message(m) {}
	
	static CommandResult Success() { return CommandResult(true); }
	static CommandResult Success(const std::string& msg) { return CommandResult(true, msg); }
	static CommandResult Failure(const std::string& msg) { return CommandResult(false, msg); }
};

using CommandHandler = std::function<CommandResult(
	ClientConnection*,
	const std::vector<std::string>&,
	GameContext&
)>;

struct CommandNode {
	std::unordered_map<std::string, std::unique_ptr<CommandNode>> children;
	std::string fullPath;
	CommandHandler handler;
	PermissionLevel minPermission;
	std::vector<std::string> aliases;
	bool hasHandler = false;
	
	CommandNode* FindOrCreateChild(const std::string& word);
	CommandNode* FindChild(const std::string& word) const;
};

class CommandTrie {
public:
	CommandTrie();
	~CommandTrie();
	
	void Insert(const std::vector<std::string>& path,
				CommandHandler handler,
				PermissionLevel minPerm);
	
	// Returns matched node + remaining words
	struct MatchResult {
		CommandNode* node = nullptr;
		size_t matchedWords = 0;
		std::vector<std::string> remaining;
		bool found = false;
	};
	
	MatchResult Match(const std::vector<std::string>& words) const;
	
	// For autocomplete/protocol
	void CollectAll(std::vector<std::string>& out) const;
	
	// Get all commands at or above permission level
	void CollectByPermission(std::vector<CommandNode*>& out, PermissionLevel level) const;
	
private:
	void CollectAllRecursive(const CommandNode* node, std::vector<std::string>& out) const;
	void CollectByPermissionRecursive(const CommandNode* node, 
									  std::vector<CommandNode*>& out, 
									  PermissionLevel level) const;
	
	std::unique_ptr<CommandNode> root_;
};
