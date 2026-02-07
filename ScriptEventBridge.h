#pragma once
#include "EventBus.h"
#include "ScriptManager.h"
class ScriptEventBridge
{
	EventBus* bus;
    ScriptManager* scripts;
public:
    ScriptEventBridge(EventBus* b, ScriptManager* s) : bus(b), scripts(s) {
        // --- BRIDGE: ROOM ENTERED ---
        bus->Subscribe(EventType::RoomEntered, [this](const EventContext& ctx) {
            auto data = std::get<RoomEventData>(ctx.data);

            // Create a Lua table to pass to the script
            sol::table luaTable = scripts->lua.create_table();
            luaTable["entity_id"] = data.EntityID;
            luaTable["room_id"] = data.RoomID;

            scripts->dispatch_event("RoomEntered", luaTable);
            });
        bus->Subscribe(EventType::CombatHit, [this](const EventContext& ctx) {
            auto data = std::get<CombatEventData>(ctx.data);

            sol::table luaTable = scripts->lua.create_table();
            luaTable["attacker_id"] = data.attackerID;
            luaTable["victim_id"] = data.victimID;

            scripts->dispatch_event("CombatHit", luaTable);
            });
    }
    ~ScriptEventBridge() = default;

private:

};
