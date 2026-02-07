#pragma once
#include <sol/sol.hpp>
#include <string>
#include <map>
#include <vector>
#include <iostream> 

class Registry;
class ClientConnection;
struct StatComponent;
class SkillResult;
class InteractableContext;
struct VitalsChangedComponent;
struct ClientComponent;
class ScriptManager;

class ScriptManager {
public:
	sol::state lua;
	Registry& registry;
	std::map<std::string, std::vector<sol::function>> event_listeners;

	ScriptManager(Registry& r);
	~ScriptManager() = default;

	void init();
	void dispatch_event(const std::string& event_name, sol::table data);
	void load_script(const std::string& path);
	void load_all_scripts(const std::string& root_path);

	template<typename... Args>
	void execute_hook(const std::string& func_name, Args&&... args) {
		if (func_name.empty()) return;

		sol::protected_function func = lua[func_name];
		if (!func.valid()) {
			return;
		}

		auto result = func(std::forward<Args>(args)...);

		if (!result.valid()) {
			sol::error err = result;
			std::cerr << "Lua Hook Error [" << func_name << "]: " << err.what() << std::endl;
		}
	}

	template<typename... Args>
	void execute_hook(sol::protected_function func, Args&&... args) {
		if (!func.valid()) {
			return;
		}

		auto result = func(std::forward<Args>(args)...);

		if (!result.valid()) {
			sol::error err = result;
			std::cerr << "Lua Runtime Error: " << err.what() << std::endl;
		}
	}

	SkillResult ExecuteSkillScript(const std::string& scriptPath, const SkillContext& ctx);
	InteractableResult ExecuteInteractableScript(const std::string& scriptPath, const std::string& functionName, const InteractableContext& context);
	ClientConnection* GetPlayer(int player_id);
};