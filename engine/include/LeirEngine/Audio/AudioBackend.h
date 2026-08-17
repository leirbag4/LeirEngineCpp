#pragma once

#include "LeirEngine/Audio/AudioTypes.h"
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector3.h"

#include <string>

namespace Leir {

// Audio backend abstraction. The engine API is backend-neutral: no SoLoud (or
// any other library) types leak into the public headers — everything is a
// SoundId/ClipId handle. Implementations: SoLoudBackend (WASAPI desktop,
// Null on CI, MiniAudio->WebAudio on web).
class LEIR_API IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    // Opens the output device. On web this does NOT start the device (the
    // browser AudioContext is born suspended); call WakeUp() from the first
    // user gesture.
    virtual bool Init() = 0;
    virtual void Shutdown() = 0;
    virtual void Update(float deltaTime) = 0;
    // (web) starts the output device on the first user gesture. No-op on desktop.
    virtual bool WakeUp() = 0;

    // --- Clips (assets, decoded once) ---
    virtual ClipId LoadClip(const std::string& path) = 0;
    virtual void UnloadClip(ClipId clip) = 0;
    virtual float GetClipDuration(ClipId clip) const = 0;

    // --- Sources (voices) ---
    virtual SoundId CreateSource() = 0;
    virtual void DestroySource(SoundId source) = 0;
    // spatial3D: true → positional playback at `position`, false → 2D.
    virtual void Play(SoundId source, ClipId clip, bool spatial3D, const Vector3& position) = 0;
    virtual void Stop(SoundId source) = 0;
    virtual void Pause(SoundId source) = 0;
    virtual void Resume(SoundId source) = 0;
    virtual void Seek(SoundId source, double seconds) = 0;
    virtual void SetLooping(SoundId source, bool loop) = 0;
    virtual void SetVolume(SoundId source, float volume) = 0;
    virtual void SetPitch(SoundId source, float pitch) = 0;
    virtual void FadeVolume(SoundId source, float targetVolume, float seconds) = 0;

    // --- 3D audio ---
    // Moves a playing 3D source; also marks the source as spatial.
    virtual void SetSource3D(SoundId source, const Vector3& position) = 0;
    // Attenuation falloff (only affects spatial sources).
    virtual void SetSource3DFalloff(SoundId source, float minDistance, float maxDistance) = 0;
    virtual void SetListener3D(const Vector3& position, const Vector3& forward, const Vector3& up) = 0;
    virtual bool Supports3D() const = 0;

    // --- Query ---
    virtual SoundState GetState(SoundId source) const = 0;
    virtual double GetTime(SoundId source) const = 0;

    // --- Global mixing ---
    virtual void SetMasterVolume(float volume) = 0;
    virtual float GetMasterVolume() const = 0;
};

} // namespace Leir