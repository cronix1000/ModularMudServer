#pragma once
#include "Registry.h"
#include "InteractionOverrideComponent.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace InteractionMessage {

struct ResolveResult {
	std::string message;
	bool fromOverride = false;
};

void ExpandTokens(std::string& message,
	const std::unordered_map<std::string, std::string>& tokens);

std::optional<ResolveResult> Resolve(
	Registry& registry,
	int entityID,
	const std::string& action,
	const std::unordered_map<std::string, std::string>& tokens = {},
	int tickCounter = 0);

}