#include "CommandTrie.h"
#include <algorithm>

CommandNode* CommandNode::FindOrCreateChild(const std::string& word) {
	auto it = children.find(word);
	if (it != children.end()) {
		return it->second.get();
	}
	
	auto newNode = std::make_unique<CommandNode>();
	CommandNode* ptr = newNode.get();
	children[word] = std::move(newNode);
	return ptr;
}

CommandNode* CommandNode::FindChild(const std::string& word) const {
	auto it = children.find(word);
	if (it != children.end()) {
		return it->second.get();
	}
	return nullptr;
}

CommandTrie::CommandTrie() : root_(std::make_unique<CommandNode>()) {}

CommandTrie::~CommandTrie() = default;

void CommandTrie::Insert(const std::vector<std::string>& path,
						 CommandHandler handler,
						 PermissionLevel minPerm) {
	if (path.empty()) return;
	
	CommandNode* current = root_.get();
	std::string fullPath;
	
	for (size_t i = 0; i < path.size(); ++i) {
		if (i > 0) fullPath += " ";
		fullPath += path[i];
		
		current = current->FindOrCreateChild(path[i]);
	}
	
	current->handler = handler;
	current->minPermission = minPerm;
	current->fullPath = fullPath;
	current->hasHandler = true;
}

CommandTrie::MatchResult CommandTrie::Match(const std::vector<std::string>& words) const {
	MatchResult result;
	
	if (words.empty() || !root_) {
		return result;
	}
	
	const CommandNode* current = root_.get();
	size_t matched = 0;
	CommandNode* lastMatch = nullptr;
	size_t lastMatchCount = 0;
	
	for (size_t i = 0; i < words.size(); ++i) {
		const CommandNode* child = current->FindChild(words[i]);
		if (!child) {
			break;
		}
		
		current = child;
		matched++;
		
		if (current->hasHandler) {
			lastMatch = const_cast<CommandNode*>(current);
			lastMatchCount = matched;
		}
	}
	
	if (lastMatch) {
		result.found = true;
		result.node = lastMatch;
		result.matchedWords = lastMatchCount;
		
		// Collect remaining words
		for (size_t i = lastMatchCount; i < words.size(); ++i) {
			result.remaining.push_back(words[i]);
		}
	}
	
	return result;
}

void CommandTrie::CollectAll(std::vector<std::string>& out) const {
	if (root_) {
		CollectAllRecursive(root_.get(), out);
	}
}

void CommandTrie::CollectAllRecursive(const CommandNode* node, std::vector<std::string>& out) const {
	if (node->hasHandler && !node->fullPath.empty()) {
		out.push_back(node->fullPath);
	}
	
	for (const auto& pair : node->children) {
		CollectAllRecursive(pair.second.get(), out);
	}
}

void CommandTrie::CollectByPermission(std::vector<CommandNode*>& out, PermissionLevel level) const {
	if (root_) {
		CollectByPermissionRecursive(root_.get(), out, level);
	}
}

void CommandTrie::CollectByPermissionRecursive(const CommandNode* node, 
											   std::vector<CommandNode*>& out, 
											   PermissionLevel level) const {
	if (node->hasHandler && node->minPermission <= level) {
		out.push_back(const_cast<CommandNode*>(node));
	}
	
	for (const auto& pair : node->children) {
		CollectByPermissionRecursive(pair.second.get(), out, level);
	}
}
