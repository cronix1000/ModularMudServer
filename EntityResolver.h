#pragma once
#include "Registry.h"
#include <vector>
#include <string>
#include <functional>

class EntityResolver {
public:
    using Shape = std::function<bool(int dx, int dy)>;

    explicit EntityResolver(Registry* registry);

    std::vector<EntityID> FindInRoom(int roomId, const std::vector<std::string>& queryTokens);
    std::vector<EntityID> FindByShape(int roomId, int centerX, int centerY,
                                      const std::vector<std::string>& queryTokens,
                                      Shape shape);
    std::vector<EntityID> ScanInShape(int roomId, int centerX, int centerY, Shape shape);

    // More a helper functions for admins looking for an item
    std::vector<EntityID> FindInGlobal(int roomId, const std::vector<std::string> queryTokens) const;

    static Shape ShapeAtTile();
    static Shape ShapeAround();
    static Shape ShapeRay(int directionX, int directionY, int maxLength);

private:
    Registry* registry_;
};