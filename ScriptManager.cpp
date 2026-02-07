#include "ScriptManager.h"
#include "Registry.h"
#include "Component.h"
#include "ClientConnection.h"
#include "InteractableContext.h"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

ScriptManager::ScriptManager(Registry& r) : registry(r) {
	// Initialize lua state in constructor
	lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::table);

	lua.new_usertype<StatComponent>("Stats",
		"hp", &StatComponent::Health,
		"strength", &StatComponent::Strength,
		"armour", &StatComponent::Armour
	);

	lua.new_usertype<SkillContext>("SkillContext",
		"source", &SkillContext::sourceID,
		"target", &SkillContext::targetID,
		"skillID", &SkillContext::skillID,   // <-- Very important for generic scripts!
		"mastery", &SkillContext::masteryLevel,
		"power", &SkillContext::basePower
	);

	lua.new_usertype<InteractableContext>("InteractableContext",
		"interactableID", &InteractableContext::interactableID,
		"userID", &InteractableContext::userID,
		"roomID", &InteractableContext::roomID,
		"x", &InteractableContext::x,
		"y", &InteractableContext::y,
		"action", &InteractableContext::action,
		"parameters", &InteractableContext::parameters
	);

	//init();
}

void ScriptManager::init() {
	load_script("scripts/interactables/interactables_master.lua");
	load_script("scripts/skills/skills_master.lua");


	lua.set_function("send_to_char", [this](int player_id, const std::string& message) {
		auto* player = GetPlayer(player_id);
		if (player) {
			player->QueueMessage(message);
		}
		});

	lua.set_function("subscribe", [this](const std::string& event_name, sol::function callback) {
		event_listeners[event_name].push_back(callback);
		});

	lua.set_function("mark_dirty", [this](int entity_id) {
		registry.AddComponent<VitalsChangedComponent>(entity_id);
		});

	lua.set_function("get_stats", [this](int entity_id) -> StatComponent* {
		return registry.GetComponent<StatComponent>(entity_id);
		});
}

void ScriptManager::dispatch_event(const std::string& event_name, sol::table data) {
	if (event_listeners.count(event_name)) {
		for (auto& func : event_listeners[event_name]) {
			auto result = func(data);
			if (!result.valid()) {
				sol::error err = result;
				std::cerr << "Lua Event Error [" << event_name << "]: " << err.what() << std::endl;
			}
		}
	}
}

SkillResult ScriptManager::ExecuteSkillScript(const std::string& scriptPath, const SkillContext& ctx) {
	// 1. Load Script
	sol::load_result script = lua.load_file(scriptPath);
	if (!script.valid()) {
		std::cerr << "[LUA ERROR] Load failed: " << scriptPath << std::endl;
		return SkillResult{ false };
	}

	// 2. Initialize Script (Runs global scope)
	sol::protected_function_result scriptBody = script();
	if (!scriptBody.valid()) {
		std::cerr << "[LUA ERROR] Exec failed: " << scriptPath << std::endl;
		return SkillResult{ false };
	}

	// 3. Find Function
	sol::protected_function func = lua["on_execute"];
	if (!func.valid()) {
		std::cerr << "[LUA ERROR] No 'on_execute' in " << scriptPath << std::endl;
		return SkillResult{ false };
	}

	// 4. Execute with Context
	auto result = func(ctx);

	// 5. Unpack Result
	if (result.valid() && result.return_count() > 0 && result[0].is<sol::table>()) {
		sol::table tbl = result[0];
		SkillResult res;

		res.success = tbl.get_or("success", false);
		res.actionType = tbl.get_or<std::string>("actionType", "none");
		res.magnitude = tbl.get_or<float>("magnitude", 0.0);
		res.damageType = tbl.get_or<std::string>("damageType", "physical");
		res.dataString = tbl.get_or<std::string>("dataString", "");

		sol::object tagsObj = tbl["addedTags"];
		if (tagsObj.is<sol::table>()) {
			sol::table tagsTbl = tagsObj;
			for (auto& pair : tagsTbl) {
				if (pair.second.is<std::string>()) {
					res.addedTags.push_back(pair.second.as<std::string>());
				}
			}
		}
		return res;
	}

	return SkillResult{ false };
}

void ScriptManager::load_script(const std::string& path) {
	auto result = lua.script_file(path);
	if (!result.valid()) {
		sol::error err = result;
		std::cerr << "Failed to load " << path << ": " << err.what() << std::endl;
	}
}

void ScriptManager::load_all_scripts(const std::string& root_path) {
	try {
		if (!fs::exists(root_path) || !fs::is_directory(root_path)) {
			std::cerr << "Script directory not found: " << root_path << std::endl;
			return;
		}

		for (const auto& entry : fs::recursive_directory_iterator(root_path)) {
			if (entry.is_regular_file() && entry.path().extension() == ".lua") {
				std::string path = entry.path().string();
				std::cout << "Loading script: " << path << std::endl;
				load_script(path);
			}
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Error scanning scripts: " << e.what() << std::endl;
	}
}

InteractableResult ScriptManager::ExecuteInteractableScript(const std::string& scriptPath, const std::string& functionName, const InteractableContext& context) {
	// 1. Load Script
	sol::load_result script = lua.load_file(scriptPath);
	if (!script.valid()) {
		std::cerr << "[LUA ERROR] Load failed: " << scriptPath << std::endl;
		return InteractableResult{ false };
	}

	// 2. Initialize Script (Runs global scope)
	sol::protected_function_result scriptBody = script();
	if (!scriptBody.valid()) {
		std::cerr << "[LUA ERROR] Exec failed: " << scriptPath << std::endl;
		return InteractableResult{ false };
	}

	// 3. Find Function
	sol::protected_function func = lua[functionName];
	if (!func.valid()) {
		std::cerr << "[LUA ERROR] No '" << functionName << "' in " << scriptPath << std::endl;
		return InteractableResult{ false };
	}

	// 4. Execute with Context
	auto result = func(context);

	// 5. Unpack Result
	if (result.valid() && result.return_count() > 0 && result[0].is<sol::table>()) {
		sol::table tbl = result[0];
		InteractableResult res;

		res.success = tbl.get_or("success", false);
		res.actionType = tbl.get_or<std::string>("actionType", "none");
		res.message = tbl.get_or<std::string>("message", "");
		res.roomMessage = tbl.get_or<std::string>("roomMessage", "");
		
		// Teleport data
		res.targetRoomID = tbl.get_or("targetRoomID", -1);
		res.targetX = tbl.get_or("targetX", -1);
		res.targetY = tbl.get_or("targetY", -1);
		
		// Item spawning
		res.spawnItemID = tbl.get_or<std::string>("spawnItemID", "");
		res.spawnX = tbl.get_or("spawnX", -1);
		res.spawnY = tbl.get_or("spawnY", -1);
		
		// Event triggering
		res.eventName = tbl.get_or<std::string>("eventName", "");
		res.newState = tbl.get_or<std::string>("newState", "");
		res.consumeOnUse = tbl.get_or("consumeOnUse", false);

		// Parse event parameters array
		sol::object paramsObj = tbl["eventParams"];
		if (paramsObj.is<sol::table>()) {
			sol::table paramsTbl = paramsObj;
			for (auto& pair : paramsTbl) {
				if (pair.second.is<std::string>()) {
					res.eventParams.push_back(pair.second.as<std::string>());
				}
			}
		}

		return res;
	}

	return InteractableResult{ false };
}

ClientConnection* ScriptManager::GetPlayer(int player_id) {
	auto* comp = registry.GetComponent<ClientComponent>(player_id);
	return comp ? comp->client : nullptr;
}