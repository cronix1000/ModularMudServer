#pragma once

#include <vector>
#include <string>
#include <cstdint>

struct PermissionComponent {
	uint8_t level = 0;  // 0 = Guest, 10 = Player, 50 = Moderator, 100 = Admin
	std::vector<std::string> roles;
	
	PermissionComponent() = default;
	PermissionComponent(uint8_t lvl) : level(lvl) {}
	PermissionComponent(uint8_t lvl, const std::vector<std::string>& r) 
		: level(lvl), roles(r) {}
	
	bool HasLevel(uint8_t required) const {
		return level >= required;
	}
	
	bool HasRole(const std::string& role) const {
		for (const auto& r : roles) {
			if (r == role) return true;
		}
		return false;
	}
};
