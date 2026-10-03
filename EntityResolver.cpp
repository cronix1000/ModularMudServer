#include "EntityResolver.h"
#include "NameComponent.h"
#include "PositionComponent.h"
#include "InventoryComponent.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>

namespace {
    const std::set<std::string> kStopWords = {"a", "the", "an", "of"};

    std::vector<std::string> Normalize(const std::vector<std::string>& raw) {
        std::vector<std::string> out;
        out.reserve(raw.size());
        for (const auto& t : raw) {
            std::string lower = t;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return std::tolower(c); });
            if (lower.empty()) continue;
            if (kStopWords.count(lower)) continue;
            out.push_back(std::move(lower));
        }
        return out;
    }
}

EntityResolver::EntityResolver(Registry* registry) : registry_(registry) {}

EntityResolver::Shape EntityResolver::ShapeAtTile() {
    return [](int dx, int dy) { return dx == 0 && dy == 0; };
}

EntityResolver::Shape EntityResolver::ShapeAround() {
    return [](int dx, int dy) {
        return std::abs(dx) <= 1 && std::abs(dy) <= 1;
    };
}

EntityResolver::Shape EntityResolver::ShapeRay(int directionX, int directionY, int maxLength) {
    return [directionX, directionY, maxLength](int dx, int dy) {
        if (maxLength <= 0) return false;
        if (directionX == 0 && directionY == 0) return false;
        if (directionX != 0 && directionY != 0) {
            if (std::abs(dx) != std::abs(dy)) return false;
            if (directionX > 0 && dx <= 0) return false;
            if (directionX < 0 && dx >= 0) return false;
            if (directionY > 0 && dy <= 0) return false;
            if (directionY < 0 && dy >= 0) return false;
            int steps = std::abs(dx);
            return steps >= 1 && steps <= maxLength;
        }
        if (directionX != 0) {
            if (dy != 0) return false;
            if (directionX > 0 && dx <= 0) return false;
            if (directionX < 0 && dx >= 0) return false;
            return std::abs(dx) >= 1 && std::abs(dx) <= maxLength;
        }
        if (dx != 0) return false;
        if (directionY > 0 && dy <= 0) return false;
        if (directionY < 0 && dy >= 0) return false;
        return std::abs(dy) >= 1 && std::abs(dy) <= maxLength;
    };
}

std::vector<EntityID> EntityResolver::FindInRoom(int roomId, const std::vector<std::string>& queryTokens) {
    return FindByShape(roomId, 0, 0, queryTokens,
                    [](int, int) { return true; });
}

std::vector<EntityID> EntityResolver::FindByShape(int roomId, int centerX, int centerY,
                                                  const std::vector<std::string>& queryTokens,
                                                  Shape shape) {
    if (!registry_) return {};
    std::vector<std::string> q = Normalize(queryTokens);
    if (q.empty()) return {};

    std::vector<EntityID> results;
    for (EntityID e : registry_->view<NameComponent>()) {
        auto* pos = registry_->GetComponent<PositionComponent>(e);
        if (!pos) continue;
        if (pos->roomId != roomId) continue;
        if (!shape(pos->x - centerX, pos->y - centerY)) continue;
        if (registry_->HasComponent<InventoryComponent>(e)) continue;

        auto* name = registry_->GetComponent<NameComponent>(e);
        if (!name) continue;
        if (name->MatchesPhrase(q)) {
            results.push_back(e);
        }
    }
    std::sort(results.begin(), results.end());
    return results;
}

std::vector<EntityID> EntityResolver::ScanInShape(int roomId, int centerX, int centerY, Shape shape) {
    if (!registry_) return {};

    std::vector<EntityID> results;
    for (EntityID e : registry_->view<NameComponent>()) {
        auto* pos = registry_->GetComponent<PositionComponent>(e);
        if (!pos) continue;
        if (pos->roomId != roomId) continue;
        if (!shape(pos->x - centerX, pos->y - centerY)) continue;
        if (registry_->HasComponent<InventoryComponent>(e)) continue;
        results.push_back(e);
    }
    std::sort(results.begin(), results.end());
    return results;
}

std::vector<EntityID> EntityResolver::FindInGlobal(int roomId, const std::vector<std::string> queryTokens) const {
    (void)roomId;
    (void)queryTokens;
    return {};
}