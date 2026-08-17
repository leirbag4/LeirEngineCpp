#include "LeirEngine/Components/AudioListener.h"

#include "LeirEngine/Audio/AudioBackend.h"
#include "LeirEngine/Audio/AudioEngine.h"
#include "LeirEngine/Core/CoreObject.h"
#include "LeirEngine/Core/Transform.h"

namespace Leir {

void AudioListener::OnUpdate(float deltaTime)
{
    (void)deltaTime;
    IAudioBackend* backend = AudioEngine::GetInstance().GetBackend();
    if (!backend)
        return;

    Transform& t = GetOwner()->GetTransform();
    backend->SetListener3D(t.GetWorldPosition(), t.GetForward(), t.GetUp());
}

} // namespace Leir