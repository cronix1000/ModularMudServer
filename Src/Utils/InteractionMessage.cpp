#include "InteractionMessage.h"
#include <functional>

namespace InteractionMessage {

namespace {
size_t PickIndex(size_t poolSize, int entityID, const std::string& action, int tick) {
	if (poolSize == 0) return 0;
	size_t h = std::hash<int>{}(entityID);
	h ^= std::hash<std::string>{}(action) + 0x9e3779b9 + (h << 6) + (h >> 2);
	h ^= std::hash<int>{}(tick) + 0x9e3779b9 + (h << 6) + (h >> 2);
	return h % poolSize;
}
}

void ExpandTokens(std::string& m, const std::unordered_map<std::string, std::string>& tokens) {
	for (const auto& [k, v] : tokens) {
		std::string token = "{" + k + "}";
		size_t pos = 0;
		while ((pos = m.find(token, pos)) != std::string::npos) {
			m.replace(pos, token.size(), v);
			pos += v.size();
		}
	}
}

std::optional<ResolveResult> Resolve(
	Registry& registry,
	int entityID,
	const std::string& action,
	const std::unordered_map<std::string, std::string>& tokens,
	int tickCounter)
{
	auto* comp = registry.GetComponent<InteractionOverrideComponent>(entityID);
	if (!comp) return std::nullopt;

	auto it = comp->overrides.find(action);
	if (it == comp->overrides.end() || it->second.empty()) return std::nullopt;

	const auto& pool = it->second;
	ResolveResult r;
	r.message = pool[PickIndex(pool.size(), entityID, action, tickCounter)];
	r.fromOverride = true;
	ExpandTokens(r.message, tokens);
	return r;
}

}