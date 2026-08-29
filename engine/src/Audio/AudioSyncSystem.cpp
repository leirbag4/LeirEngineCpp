#include "LeirEngine/Audio/AudioSyncSystem.h"
#include "LeirEngine/Audio/AudioBackend.h"
#include "LeirEngine/Audio/AudioEngine.h"
#include "LeirEngine/Components/AudioSource.h"
#include "LeirEngine/Components/AudioListener.h"
#include "LeirEngine/ECS/World.h"
#include "LeirEngine/ECS/TransformSystem.h"

namespace Leir {

void AudioSyncSystem::Update(ECS::World& world)
{
    IAudioBackend* backend = AudioEngine::GetInstance().GetBackend();
    if (!backend)
        return;

    // Listener: push the world pos + orientation (forward/up from the world rot).
    world.Each<AudioListener, ECS::WorldTransform>([&](AudioListener&, ECS::WorldTransform& wt, ECS::Entity) {
        backend->SetListener3D(
            wt.worldPosition,
            wt.worldRotation * Vector3::Forward(),
            wt.worldRotation * Vector3::Up());
    });

    // Sources: play-on-awake + per-frame 3D position sync.
    world.Each<AudioSource, ECS::WorldTransform>([&](AudioSource& src, ECS::WorldTransform& wt, ECS::Entity) {
        src.AutoStart();
        src.Sync3D(wt.worldPosition);
    });
}

} // namespace Leir