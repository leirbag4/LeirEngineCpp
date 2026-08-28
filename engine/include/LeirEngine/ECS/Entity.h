#pragma once

#include "LeirEngine/Core/Export.h"

#include <cstdint>
#include <functional>

namespace Leir {
namespace ECS {

inline constexpr uint32_t kNullIndex = 0;

// Generational entity handle: an index into the world's entity arrays plus a
// generation counter, so a stale handle (destroyed entity) never resolves to a
// recycled one. index 0 is reserved as the null entity (like EnTT).
struct LEIR_API Entity {
    uint32_t index = 0;
    uint32_t generation = 0;

    bool operator==(const Entity& o) const { return index == o.index && generation == o.generation; }
    bool operator!=(const Entity& o) const { return !(*this == o); }
    explicit operator bool() const { return index != kNullIndex && generation != 0; }
};

inline constexpr Entity kNullEntity = {kNullIndex, 0};

} // namespace ECS
} // namespace Leir

namespace std {
template<>
struct hash<Leir::ECS::Entity> {
    size_t operator()(const Leir::ECS::Entity& e) const noexcept {
        return (size_t(e.generation) << 32) | e.index;
    }
};
} // namespace std