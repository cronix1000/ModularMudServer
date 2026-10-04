#include "MetaRegistry.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace MetaRegistry {

namespace {

const EntitySpec* FindEntity(const std::string& entityType) {
	for (const auto& e : ENTITY_SPECS) {
		if (entityType == e.entityType) return &e;
	}
	return nullptr;
}

const KeySpec* FindKey(const EntitySpec& e, const std::string& key) {
	for (int i = 0; i < e.keyCount; ++i) {
		if (key == e.keys[i].key) return &e.keys[i];
	}
	return nullptr;
}

nlohmann::json DefaultValue(const KeySpec& spec) {
	switch (spec.type) {
		case KeyType::Int:    return static_cast<int>(spec.defaultDouble);
		case KeyType::Float:  return spec.defaultDouble;
		case KeyType::Bool:   return spec.defaultBool;
		case KeyType::String: return spec.defaultString;
	}
	return nlohmann::json(nullptr);
}

bool Validate(const KeySpec& spec, const nlohmann::json& v) {
	switch (spec.type) {
		case KeyType::Int:
			if (!v.is_number_integer()) return false;
			if (spec.hasMin && v.get<int>() < static_cast<int>(spec.minVal)) return false;
			if (spec.hasMax && v.get<int>() > static_cast<int>(spec.maxVal)) return false;
			return true;
		case KeyType::Float:
			if (!v.is_number()) return false;
			if (spec.hasMin && v.get<double>() < spec.minVal) return false;
			if (spec.hasMax && v.get<double>() > spec.maxVal) return false;
			return true;
		case KeyType::Bool:
			return v.is_boolean();
		case KeyType::String:
			if (!v.is_string()) return false;
			if (spec.hasMaxLen && static_cast<int>(v.get<std::string>().size()) > spec.maxLen) return false;
			return true;
	}
	return false;
}

}

bool ApplyDefaults(const std::string& entityType, nlohmann::json& meta) {
	const EntitySpec* entity = FindEntity(entityType);
	if (!entity) return false;

	bool changed = false;

	for (int i = 0; i < entity->keyCount; ++i) {
		const KeySpec& spec = entity->keys[i];
		auto it = meta.find(spec.key);
		if (it == meta.end()) {
			meta[spec.key] = DefaultValue(spec);
			changed = true;
			continue;
		}
		if (!Validate(spec, *it)) {
			std::cerr << "[MetaRegistry] Invalid meta value for " << entityType
			          << "." << spec.key << "; replacing with default." << std::endl;
			*it = DefaultValue(spec);
			changed = true;
		}
	}
	return changed;
}

} // namespace MetaRegistry