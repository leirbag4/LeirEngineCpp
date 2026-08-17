#pragma once

#include "LeirEngine/Core/Component.h"
#include "LeirEngine/Core/Export.h"

namespace Leir {

// 3D audio listener. Every frame (OnUpdate) it pushes its world position and
// orientation (forward/up from the owning Transform) into the audio backend.
// Attach to the camera (or any object) to hear spatialized AudioSources.
class LEIR_API AudioListener : public Component {
public:
    void OnUpdate(float deltaTime) override;
};

} // namespace Leir