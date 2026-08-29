#pragma once

#include "LeirEngine/Core/Export.h"

namespace Leir {
namespace ECS { class World; }

// Drives the audio data components (Incremento 3, TODO_HYBRID_ECS.md §10):
// pushes the AudioListener world pos/orientation into the backend and syncs
// each AudioSource's 3D position + play-on-awake. Replaces the old
// AudioSource/AudioListener OnUpdate lifecycle. Runs in Scene::OnUpdate.
class LEIR_API AudioSyncSystem {
public:
    void Update(ECS::World& world);
};

} // namespace Leir