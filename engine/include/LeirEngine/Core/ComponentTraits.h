#pragma once

#include "LeirEngine/Core/Export.h"

#include <type_traits>

namespace Leir {

// Marker for components stored DIRECTLY in the ECS pools (data-oriented, no
// HybridComponent box — Incremento 1, TODO_HYBRID_ECS.md §10). Their data is
// contiguous in the pool (cache-friendly; SoA/SIMD-ready in Fase 2). Each data
// component header specializes this to std::true_type (it still derives
// Component so GetOwner/OnAwake etc. keep working; the methods are the API).
template<typename T>
struct LEIR_API IsDataComponent : std::false_type {
};

} // namespace Leir