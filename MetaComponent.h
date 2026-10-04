#pragma once
#include <nlohmann/json.hpp>
#include <string>

struct MetaComponent {
	nlohmann::json meta = nlohmann::json::object();

	bool has(const std::string& key) const {
		return meta.contains(key);
	}
};