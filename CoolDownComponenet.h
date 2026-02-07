#pragma once
#include <map>
#include <string>

struct CoolDownComponent {
	std::map<std::string, float> cooldowns;
	void set(std::string key, float MAX) {

	}
};