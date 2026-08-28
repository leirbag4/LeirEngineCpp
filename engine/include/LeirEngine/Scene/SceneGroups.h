#pragma once

#include "LeirEngine/ECS/OwnedGroup.h"
#include "LeirEngine/ECS/HybridComponent.h"
#include "LeirEngine/ECS/TransformSystem.h"
#include "LeirEngine/ECS/Tags.h"
#include "LeirEngine/Components/MeshRenderer.h"
#include "LeirEngine/Components/Camera.h"
#include "LeirEngine/Components/Light.h"
#include "LeirEngine/Components/SpriteRenderer.h"

namespace Leir {
namespace SceneGroups {

// Journal-synced query groups over the ECS data (resto c, TODO_HYBRID_ECS.md
// §4.4/§7): the renderer consumes these directly — contiguous member iteration,
// no GetObjects/GetComponent/GetTransform per frame. Membership = the entity
// owns the component + Active + a WorldTransform.
using Renderables = ECS::OwnedGroup<ECS::HybridComponent<MeshRenderer>, ECS::Active, ECS::WorldTransform>;
using Sprites    = ECS::OwnedGroup<ECS::HybridComponent<SpriteRenderer>, ECS::Active, ECS::WorldTransform>;
using Cameras    = ECS::OwnedGroup<ECS::HybridComponent<Camera>, ECS::Active, ECS::WorldTransform>;
using Lights     = ECS::OwnedGroup<ECS::HybridComponent<Light>, ECS::Active, ECS::WorldTransform>;

} // namespace SceneGroups
} // namespace Leir