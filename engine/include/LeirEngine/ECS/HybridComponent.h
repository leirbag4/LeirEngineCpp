#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Core/Component.h"

#include <memory>
#include <utility>

namespace Leir {
namespace ECS {

// Boxes a classic OOP Component (T : Leir::Component) inside the ECS so the
// friendly `AddComponent<T>()` semantics survive on top of data-oriented
// storage (Unity DOTS "hybrid component" pattern adapted). The ECS owns the
// instance; removing the entity/component destroys it, and the box calls
// OnDestroy() first. Move-only (unique_ptr) — TypedPool handles move-only
// elements (emplace + move-assign + pop).
template<typename T>
struct HybridComponent {
    static_assert(std::is_base_of_v<Component, T>, "HybridComponent<T> requires T : Component");

    HybridComponent() = default;
    HybridComponent(HybridComponent&&) = default;
    HybridComponent& operator=(HybridComponent&&) = default;
    HybridComponent(const HybridComponent&) = delete;
    HybridComponent& operator=(const HybridComponent&) = delete;

    ~HybridComponent()
    {
        if (instance)
            instance->OnDestroy();
    }

    std::unique_ptr<T> instance;
};

} // namespace ECS
} // namespace Leir