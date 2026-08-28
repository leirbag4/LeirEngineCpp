#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/ECS/Entity.h"
#include "LeirEngine/ECS/ComponentPool.h"
#include "LeirEngine/ECS/HybridComponent.h"

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Leir {
namespace ECS {

// Runtime metadata for a registered component type (the seed of the reflection
// framework: name/size/alignment; JSON serialization comes in a later step).
struct LEIR_API ComponentTypeInfo {
    std::string name;
    size_t size = 0;
    size_t alignment = 0;
    uint32_t typeId = 0;
};

// Journal of structural changes (the Fase-0/1 hybrid, TODO_HYBRID_ECS.md §4.4):
// entities/components created/destroyed/added/removed, stamped by a monotonic
// change version. The owned SoA groups and query caches (next steps) consume
// this to stay incrementally in sync without re-scanning every frame.
struct LEIR_API ChangeRecord {
    enum Type : uint8_t {
        EntityCreated,
        EntityDestroyed,
        ComponentAdded,
        ComponentRemoved,
    };
    Type type;
    uint32_t entityIndex;
    uint32_t componentTypeId;
};

class LEIR_API World {
public:
    World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    ~World() = default;

    // --- Entities ---
    Entity Create();
    void Destroy(Entity e);
    bool IsAlive(Entity e) const;
    uint32_t GenerationOf(uint32_t entityIndex) const {
        return entityIndex < m_Generations.size() ? m_Generations[entityIndex] : 0;
    }

    // --- Component storage (sparse-set pools) ---
    template<typename T>
    uint32_t ComponentType() {
        auto key = std::type_index(typeid(T));
        auto it = m_TypeToId.find(key);
        if (it != m_TypeToId.end())
            return it->second;
        uint32_t id = (uint32_t)m_TypeInfos.size();
        m_TypeInfos.push_back({typeid(T).name(), sizeof(T), alignof(T), id});
        m_TypeToId[key] = id;
        return id;
    }

    template<typename T>
    TypedPool<T>* GetTypedPool() {
        uint32_t id = ComponentType<T>();
        if (m_Pools.size() <= id)
            m_Pools.resize((size_t)id + 1);
        if (!m_Pools[id])
            m_Pools[id] = std::make_unique<TypedPool<T>>();
        return static_cast<TypedPool<T>*>(m_Pools[id].get());
    }

    template<typename T>
    T* Get(Entity e) {
        if (!e)
            return nullptr;
        return GetTypedPool<T>()->Get(e.index);
    }

    template<typename T>
    const T* Get(Entity e) const {
        if (!e)
            return nullptr;
        return const_cast<World*>(this)->GetTypedPool<T>()->Get(e.index);
    }

    template<typename T>
    bool Has(Entity e) const {
        if (!e)
            return false;
        return const_cast<World*>(this)->GetTypedPool<T>()->Has(e.index);
    }

    // One component per type per entity (Unity/Godot semantics): returns the
    // existing instance if the entity already owns T.
    template<typename T>
    T& Add(Entity e) {
        TypedPool<T>* pool = GetTypedPool<T>();
        if (pool->Has(e.index))
            return *pool->Get(e.index);
        T& ref = pool->Add(e.index);
        m_Journal.push_back({ChangeRecord::ComponentAdded, e.index, ComponentType<T>()});
        ++m_ChangeVersion;
        return ref;
    }

    template<typename T>
    void Remove(Entity e) {
        if (!IsAlive(e))
            return;
        TypedPool<T>* pool = GetTypedPool<T>();
        if (!pool->Has(e.index))
            return;
        pool->RemoveFromEntity(e.index);
        m_Journal.push_back({ChangeRecord::ComponentRemoved, e.index, ComponentType<T>()});
        ++m_ChangeVersion;
    }

    // --- Hybrid components (OOP Component boxed in the ECS) ---
    // One per type per entity (same semantics as AddComponent<T>).
    template<typename T, typename... Args>
    T& AddHybrid(Entity e, Args&&... args) {
        auto& hc = Add<HybridComponent<T>>(e);
        if (!hc.instance)
            hc.instance = std::make_unique<T>(std::forward<Args>(args)...);
        return *hc.instance;
    }

    template<typename T>
    T* GetHybrid(Entity e) {
        auto* hc = Get<HybridComponent<T>>(e);
        return hc ? hc->instance.get() : nullptr;
    }

    // --- Iteration ---
    // Calls fn(components&..., Entity) for every entity owning ALL of Ts.
    // Iterates the first pool's dense array and joins the rest via sparse
    // membership (the sparse-set multi-type pattern).
    template<typename... Ts, typename Fn>
    void Each(Fn&& fn) {
        static_assert(sizeof...(Ts) > 0, "Each requires at least one component type");
        auto pools = std::tuple<TypedPool<Ts>*...>(GetTypedPool<Ts>()...);
        auto* first = std::get<0>(pools);
        auto& denseEntities = first->Entities();
        for (size_t di = 0; di < denseEntities.size(); ++di) {
            uint32_t ei = denseEntities[di];
            Entity e{ei, m_Generations[ei]};
            std::tuple<Ts*...> comps = std::apply(
                [ei](auto*... ps) { return std::make_tuple(ps->Get(ei)...); }, pools);
            bool all = std::apply([](auto*... ps) { return (ps && ...); }, comps);
            if (!all)
                continue;
            std::apply([&](Ts*... ps) { fn(*ps..., e); }, comps);
        }
    }

    // --- Journal ---
    const std::vector<ChangeRecord>& GetJournal() const { return m_Journal; }
    uint64_t GetChangeVersion() const { return m_ChangeVersion; }
    void ClearJournal() { m_Journal.clear(); }

private:
    friend class TypedPoolDetail;

    uint32_t AllocateIndex();

    std::vector<uint32_t> m_Generations;   // entity index -> generation
    std::vector<uint32_t> m_LiveIndices;   // dense live entity indices
    std::vector<uint32_t> m_IndexToLive;   // entity index -> slot in m_LiveIndices
    std::vector<uint32_t> m_FreeList;      // recyclable entity indices

    std::unordered_map<std::type_index, uint32_t> m_TypeToId;
    std::vector<ComponentTypeInfo> m_TypeInfos;
    std::vector<std::unique_ptr<ComponentPoolBase>> m_Pools;

    std::vector<ChangeRecord> m_Journal;
    uint64_t m_ChangeVersion = 0;
};

} // namespace ECS
} // namespace Leir