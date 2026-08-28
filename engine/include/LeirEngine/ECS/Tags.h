#pragma once

#include "LeirEngine/Core/Export.h"

namespace Leir {
namespace ECS {

// Family tags (the ECS replacement for dynamic_cast<Object3D/2D/UINode>):
// empty marker components that group entities by object family. The hierarchy
// panel and renderers filter by these instead of RTTI (TODO_HYBRID_ECS.md §7).
struct LEIR_API Tag3D {};
struct LEIR_API Tag2D {};
struct LEIR_API TagUI {};

} // namespace ECS
} // namespace Leir