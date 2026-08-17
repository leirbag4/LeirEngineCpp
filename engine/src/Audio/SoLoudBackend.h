#pragma once

#include "LeirEngine/Audio/AudioBackend.h"
#include "LeirEngine/Audio/AudioTypes.h"

#include <soloud.h>
#include <soloud_wav.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace Leir {

// IAudioBackend implementation over SoLoud. Internal header — never exposed in
// the public API. Desktop uses the WASAPI backend (SoLoud's AUTO picks it);
// Linux/macOS/CI use Null; web uses MiniAudio -> WebAudio (Init() does NOT
// start the device there — WakeUp() does, from the first user gesture).
class SoLoudBackend : public IAudioBackend {
public:
    SoLoudBackend();
    ~SoLoudBackend() override;

    bool Init() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    bool WakeUp() override;

    ClipId LoadClip(const std::string& path) override;
    void UnloadClip(ClipId clip) override;
    float GetClipDuration(ClipId clip) const override;

    SoundId CreateSource() override;
    void DestroySource(SoundId source) override;
    void Play(SoundId source, ClipId clip, bool spatial3D, const Vector3& position) override;
    void Stop(SoundId source) override;
    void Pause(SoundId source) override;
    void Resume(SoundId source) override;
    void Seek(SoundId source, double seconds) override;
    void SetLooping(SoundId source, bool loop) override;
    void SetVolume(SoundId source, float volume) override;
    void SetPitch(SoundId source, float pitch) override;
    void FadeVolume(SoundId source, float targetVolume, float seconds) override;

    void SetSource3D(SoundId source, const Vector3& position) override;
    void SetSource3DFalloff(SoundId source, float minDistance, float maxDistance) override;
    void SetListener3D(const Vector3& position, const Vector3& forward, const Vector3& up) override;
    bool Supports3D() const override;

    SoundState GetState(SoundId source) const override;
    double GetTime(SoundId source) const override;

    void SetMasterVolume(float volume) override;
    float GetMasterVolume() const override;

private:
    struct SourceRec {
        SoLoud::handle voice = 0; // invalid handle
        ClipId clip = 0;
        float volume = 1.0f;
        float pitch = 1.0f;
        bool looping = false;
        bool is3D = false;
        Vector3 position;
        float minDistance = 1.0f;
        float maxDistance = 10000.0f;
    };

    SourceRec& GetSource(SoundId source);
    bool InitDevice();

    mutable SoLoud::Soloud m_Soloud;
    bool m_Initialized = false;  // backend object exists (web: device may not be started)
    bool m_DeviceStarted = false; // SoLoud::init() ran (device audible)
    SoundId m_NextSource = 1;
    mutable std::unordered_map<ClipId, std::pair<std::string, SoLoud::Wav>> m_Clips;
    std::unordered_map<SoundId, SourceRec> m_Sources;
};

} // namespace Leir