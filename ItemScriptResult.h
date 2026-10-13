#pragma once
#include <string>

struct ItemScriptResult {
	bool success = false;
	bool cancelEquip = false;
	bool cancelUnequip = false;
	bool cancelDrop = false;
	std::string message;
	std::string roomMessage;
};
