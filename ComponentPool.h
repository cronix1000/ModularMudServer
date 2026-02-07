#pragma once

#include <vector>
#include <iostream>

// Forward declaration for EntityID if it's defined elsewhere, e.g., in Entity.h
// For now, we'll assume EntityID is a type alias for an integer type.
using EntityID = int;

/**
 * @class IComponentPool
 * @brief An interface for a generic component pool.
 *
 * This non-template base class allows the Registry to manage a collection of
 * different ComponentPool<T> types through a common pointer.
 */
class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    // A virtual method to notify the pool when an entity is destroyed.
    virtual void OnEntityDestroyed(EntityID entity) = 0;
};

/**
 * @class ComponentPool<T>
 * @brief A cache-friendly, high-performance storage for a single component type.
 *
 * This class uses a sparse set data structure to achieve O(1) amortized time complexity
 * for component access, addition, and removal, while storing component data contiguously
 * in memory for extremely fast iteration.
 *
 * @tparam T The type of the component to store.
 */
template <typename T>
class ComponentPool : public IComponentPool {
public:
    /**
     * @brief Adds a component for a given entity.
     * @return A reference to the newly added component.
     */
    template <typename... Args>
    T& Add(EntityID entity, Args&&... args) {
        if (Has(entity)) {
            return Get(entity);
        }

        // Resize sparse map if the entity ID is out of bounds
        if (entity >= sparse_map.size()) {
            sparse_map.resize(entity + 1, -1);
        }

        // Add the component and entity to the end of the dense, packed arrays
        sparse_map[entity] = components.size();
        components.emplace_back(std::forward<Args>(args)...);
        packed_entities.push_back(entity);
        return components.back();
    }

    /**
     * @brief Gets the component for a given entity.
     * @return A reference to the component.
     */
    T& Get(EntityID entity) {
        // Note: For performance, this performs no safety checks.
        // A debug build might add: if (!Has(entity)) { throw std::runtime_error("..."); }
        return components[sparse_map[entity]];
    }

    /**
     * @brief Checks if an entity has a component in this pool.
     */
    bool Has(EntityID entity) const {
        return entity < sparse_map.size() && sparse_map[entity] != -1;
    }

    /**
     * @brief Removes a component from an entity.
     */
    void Remove(EntityID entity) {
        if (!Has(entity)) {
            return;
        }

        // To remove an element from a packed array without invalidating indices,
        // we move the *last* element into the slot of the one being removed.

        // 1. Get the dense index of the component to remove.
        size_t dense_index_to_remove = sparse_map[entity];

        // 2. Get the entity and component at the end of the packed arrays.
        EntityID last_entity = packed_entities.back();
        T& last_component = components.back();

        // 3. Move the last element into the place of the one being removed.
        components[dense_index_to_remove] = std::move(last_component);
        packed_entities[dense_index_to_remove] = last_entity;

        // 4. Update the sparse map to point the moved entity to its new location.
        sparse_map[last_entity] = dense_index_to_remove;

        // 5. Invalidate the sparse entry for the removed entity.
        sparse_map[entity] = -1;

        // 6. Shrink the packed arrays.
        components.pop_back();
        packed_entities.pop_back();
    }

    // --- For the IComponentPool interface ---
    void OnEntityDestroyed(EntityID entity) override {
        if (Has(entity)) {
            Remove(entity);
        }
    }

    // --- For easy iteration ---
    std::vector<T>& GetComponents() { return components; }
    std::vector<EntityID>& GetEntities() { return packed_entities; }

private:
    // "Dense" array: Tightly packed components for cache-friendly iteration.
    std::vector<T> components;

    // "Dense" array: The entity ID corresponding to each component.
    std::vector<EntityID> packed_entities;

    // "Sparse" array: Maps an EntityID to its index in the dense arrays.
    // An index of -1 means the entity does not have the component.
    std::vector<int> sparse_map;
};
