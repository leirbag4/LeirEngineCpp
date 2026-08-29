#pragma once

#include "LeirEngine/ECS/OwnedGroup.h"
#include "LeirEngine/ECS/TransformSystem.h"
#include "LeirEngine/ECS/Tags.h"
#include "LeirEngine/Components/MeshRenderer.h"
#include "LeirEngine/Components/Camera.h"
#include "LeirEngine/Components/Light.h"
#include "LeirEngine/Components/SpriteRenderer.h"

namespace Leir {
namespace SceneGroups {

// Journal-synced query groups over the ECS data (Incremento 1,
// TODO_HYBRID_ECS.md §10): the renderer consumes these directly — contiguous
// member iteration over the POD components (no HybridComponent box, no
// GetObjects/GetComponent per frame). Membership = the entity owns the
// component + Active + a WorldTransform.
using Renderables = ECS::OwnedGroup<MeshRenderer, ECS::Active, ECS::WorldTransform>;
using Sprites    = ECS::OwnedGroup<SpriteRenderer, ECS::Active, ECS::WorldTransform>;
using Cameras    = ECS::OwnedGroup<Camera, ECS::Active, ECS::WorldTransform>;
using Lights     = ECS::OwnedGroup<Light, ECS::Active, ECS::WorldTransform>;

} // namespace SceneGroups
} // namespace Leir