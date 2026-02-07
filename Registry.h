#pragma once

#include <unordered_map>
#include <typeindex>
#include <memory>
#include <vector>
#include "ComponentPool.h" // Include our new header

/**
 * @class Registry
 * @brief A high-performance, data-oriented Entity Component System registry.
 *
 * This class is the central hub for managing entities and their components. It uses
 * ComponentPools to store component data in a cache-friendly, contiguous manner,
 * enabling fast iteration and access.
 */
class Registry {
public:
    Registry() = default;

    // --- Entity Management ---

    EntityID CreateEntity() {
        return next_id++;
    }

    void DestroyEntity(EntityID entity) {
        // Notify all component pools that this entity is being destroyed.
        for (auto const& [type, pool] : component_pools) {
            pool->OnEntityDestroyed(entity);
        }
    }

    // --- Component Management ---

    /**
     * @brief Adds a component to an entity by constructing it in-place.
     * @tparam T The component type to add.
     * @return A reference to the newly created component.
     */
    template<typename T, typename... Args>
    T& AddComponent(EntityID entity, Args&&... args) {
        return GetPool<T>()->Add(entity, std::forward<Args>(args)...);
    }

    /**
     * @brief Adds a component to an entity by copying or moving an existing component.
     * @tparam T The component type (deduced from the argument).
     * @return A reference to the newly added component.
     */
    template<typename T>
    T& AddComponent(EntityID entity, T&& component) {
        // Use std::decay_t to remove references and const-qualifiers from T,
        // so we get the actual component type for the pool lookup.
        return GetPool<std::decay_t<T>>()->Add(entity, std::forward<T>(component));
    }

    /**
     * @brief Gets a component for a given entity.
     * @return A pointer to the component, or nullptr if the entity doesn't have one.
     */
    template<typename T>
    T* GetComponent(EntityID entity) {
        auto* pool = GetPool<T>();
        if (pool->Has(entity)) {
            return &pool->Get(entity);
        }
        return nullptr;
    }

    /**
     * @brief Removes a component from an entity.
     */
    template<typename T>
    void RemoveComponent(EntityID entity) {
        GetPool<T>()->Remove(entity);
    }

    /**
     * @brief Checks if an entity has a specific component.
     */
    template<typename T>
    bool HasComponent(EntityID entity) {
        return GetPool<T>()->Has(entity);
    }

    /**
    * @brief Gets all components of a given type.
    * @return A vector of all components of that type.
    */
    template <typename T>
    std::vector<T>& GetAllComponents() {
        return GetPool<T>()->GetComponents();
    }
    
    // --- Views and Iteration ---

    /**
     * @brief Creates a view to iterate over entities that have a set of components.
     * This is a simplified view for single-component iteration.
     */
    template<typename T>
    auto& view() {
        return GetPool<T>()->GetEntities();
    }
    
private:
    EntityID next_id = 1; // Start at 1, 0 could be a null/global entity

    // A map from a component's type_index to its ComponentPool.
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> component_pools;

    /**
     * @brief Gets (or creates) the component pool for a given component type.
     */
    template<typename T>
    ComponentPool<T>* GetPool() {
        std::type_index type_id = std::type_index(typeid(T));

        if (component_pools.find(type_id) == component_pools.end()) {
            // If the pool doesn't exist, create it.
            component_pools[type_id] = std::make_unique<ComponentPool<T>>();
        }

        // Downcast the interface pointer to the concrete pool type.
        return static_cast<ComponentPool<T>*>(component_pools[type_id].get());
    }
};