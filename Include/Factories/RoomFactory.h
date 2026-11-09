#pragma once
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <string>

// Forward declarations
class GameContext;
struct RoomLayoutComponent;
struct RoomExitsComponent;

using json = nlohmann::json;
using EntityID = int;

class RoomFactory {
public:
    RoomFactory(GameContext& context);
    
    EntityID CreateRoom(const json& roomData);
    EntityID CreateInstancedRoom(int templateRoomId);
    EntityID GetRoomById(int roomId);
    
private:
    GameContext& ctx;
    std::unordered_map<int, EntityID> roomIdToEntity;
    int nextInstanceId;
    
    EntityID CreateRoomInternal(const json& roomData, bool isInstance, int templateId);
    void ParseLayout(RoomLayoutComponent& layout, const json& layoutData, const json& roomData);
    void ParseExits(RoomExitsComponent& exits, const json& exitsData);
};