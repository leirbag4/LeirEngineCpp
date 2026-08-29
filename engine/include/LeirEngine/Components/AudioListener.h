#pragma once

#include "LeirEngine/Core/Component.h"
#include "LeirEngine/Core/ComponentTraits.h"
#include "LeirEngine/Core/Export.h"

namespace Leir {

// 3D audio listener (data component, Incremento 3, TODO_HYBRID_ECS.md §10).
// A marker component: AudioSyncSystem pushes its world position + orientation
// into the audio backend every frame. Attach to the camera (or any object) to
// hear spatialized AudioSources.
class LEIR_API AudioListener : public Component {
};

} // namespace Leir

template<>
struct Leir::IsDataComponent<Leir::AudioListener> : std::true_type {
};