#pragma once

#include "LeirEngine/Audio/AudioBackend.h"
#include "LeirEngine/Audio/AudioTypes.h"
#include "LeirEngine/Core/Export.h"

#include <memory>
#include <string>

namespace Leir {

class AudioClip;

// Audio subsystem singleton (pattern: PhysicsWorld). Owns the backend and the
// clip cache. Lifecycle:
//   Init()      - creates the platform backend (does NOT start the web device)
//   WakeUp()    - (web) starts the device on the first user gesture
//   Update(dt)  - called every frame (faders + 3D listener/source refresh)
//   Shutdown()  - tears everything down
class LEIR_API AudioEngine {
public:
    static AudioEngine& GetInstance();

    void Init();
    void Shutdown();
    void Update(float deltaTime);
    void WakeUp();

    bool IsInitialized() const { return m_Initialized; }

    IAudioBackend* GetBackend() const { return m_Backend.get(); }

    // Clip cache: loads (or returns a cached) clip for a path.
    ClipId GetOrLoadClip(const std::string& path);
    std::shared_ptr<AudioClip> GetClipAsset(const std::string& path);
    void UnloadAllClips();

private:
    AudioEngine() = default;
    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    std::unique_ptr<IAudioBackend> m_Backend;
    bool m_Initialized = false;
};

} // namespace Leir