#pragma once
#include <sol/sol.hpp>
#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <nlohmann/json.hpp>


class Registry;
class ClientConnection;
struct StatComponent;
struct SkillResult;
struct InteractableResult;
struct VitalsChangedComponent;
struct ClientComponent;
class ScriptManager;
struct InteractableContext;
struct SkillContext;
struct ItemScriptContext;
struct ItemScriptResult;
struct GameContext;
#include <nlohmann/json.hpp>

class ScriptManager {
public:
	sol::state lua;
	Registry& registry;
	GameContext* gameContext = nullptr;
	std::map<std::string, std::vector<sol::function>> event_listeners;

	ScriptManager(Registry& r);
	~ScriptManager() = default;

	void init();
	void dispatch_event(const std::string& event_name, sol::table data);
	void load_script(const std::string& path);
	void load_all_scripts(const std::string& root_path);

	void GrantExperience(int playerID, int amount, const std::string& source);

	sol::table GetMagicKnown(int playerID);

	void AcceptQuest(int playerID, const std::string& questId);
	void ProgressQuest(int playerID, const std::string& questId, const std::string& objectiveId, int delta);
	void CompleteQuest(int playerID, const std::string& questId);
	bool IsQuestActive(int playerID, const std::string& questId) const;
	bool IsQuestCompleted(int playerID, const std::string& questId) const;

	nlohmann::json GetMetaForLua(int entityID) const;

	int GetFactionStanding(int playerID, const std::string& factionId);
	int AdjustFactionStanding(int playerID, const std::string& factionId, int delta);

	int GetGold(int playerID, const std::string& slot = "");
	int AddGold(int playerID, int amount, const std::string& slot = "");
	int QuoteBuyPrice(const std::string& keeperId, const std::string& templateId) const;
	int QuoteSellPrice(const std::string& keeperId, const std::string& templateId) const;
	bool ShopBuy(int playerID, const std::string& keeperId, const std::string& templateId, int qty);
	bool ShopSell(int playerID, const std::string& keeperId, const std::string& templateId, int qty);

	template<typename... Args>
	void execute_hook(const std::string& func_name, Args&&... args) {
		fprintf(stderr, "[DEBUG:execute_hook] ENTER name='%s'\n", func_name.c_str()); fflush(stderr);
		if (func_name.empty()) return;

		sol::object obj = lua[func_name];
		fprintf(stderr, "[DEBUG:execute_hook] lua['%s'] type=%d valid=%d\n",
			func_name.c_str(), (int)obj.get_type(), obj.valid() ? 1 : 0); fflush(stderr);

		sol::protected_function func = lua[func_name];
		if (!func.valid()) {
			fprintf(stderr, "[DEBUG:execute_hook] '%s' is not a valid Lua global\n", func_name.c_str()); fflush(stderr);
			return;
		}

		auto result = func(std::forward<Args>(args)...);

		if (!result.valid()) {
			sol::error err = result;
			std::cerr << "Lua Hook Error [" << func_name << "]: " << err.what() << std::endl;
		} else {
			fprintf(stderr, "[DEBUG:execute_hook] '%s' returned valid\n", func_name.c_str()); fflush(stderr);
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
	ItemScriptResult ExecuteItemScript(const std::string& scriptPath, const std::string& functionName, const ItemScriptContext& context);
	ClientConnection* GetPlayer(int player_id);
	template<typename ...Args>
	void BroadcastEvent(const std::string& eventName, Args && ...args);

private:
	std::string scriptsRoot;

	static std::string DiscoverScriptsRoot();
	std::string ResolveScriptPath(const std::string& path, const std::string& kindDir) const;
	sol::protected_function LoadOrGetScript(const std::string& path, const std::string& kindDir, const std::string& functionName);
};