#pragma once

#include "LeirEngine/Audio/AudioBackend.h"
#include "LeirEngine/Audio/AudioTypes.h"
#include "LeirEngine/Core/Component.h"
#include "LeirEngine/Core/Export.h"

#include <memory>
#include <string>

namespace Leir {

class AudioClip;

// Unity-style 3D sound emitter component. Plays a clip through the engine's
// audio backend; when Spatial3D is enabled its position is synced from the
// owning Transform every frame (OnUpdate) and the sound is 3D-positioned
// (attenuation by distance from the AudioListener). Otherwise it is 2D.
class LEIR_API AudioSource : public Component {
public:
    void SetClip(std::shared_ptr<AudioClip> clip);
    // Convenience: loads the clip through the AudioEngine cache.
    void SetClipPath(const std::string& path);
    std::shared_ptr<AudioClip> GetClip() const { return m_Clip; }

    void Play();
    void Stop();
    void Pause();
    void Resume();
    void Seek(double seconds);

    void SetLooping(bool loop) { m_Looping = loop; if (m_Initialized) GetBackend()->SetLooping(m_Source, loop); }
    bool IsLooping() const { return m_Looping; }
    void SetVolume(float volume) { m_Volume = volume; if (m_Initialized) GetBackend()->SetVolume(m_Source, volume); }
    float GetVolume() const { return m_Volume; }
    void SetPitch(float pitch) { m_Pitch = pitch; if (m_Initialized) GetBackend()->SetPitch(m_Source, pitch); }
    float GetPitch() const { return m_Pitch; }
    void SetPlayOnAwake(bool playOnAwake) { m_PlayOnAwake = playOnAwake; }
    bool GetPlayOnAwake() const { return m_PlayOnAwake; }

    void SetSpatial3D(bool spatial) { m_Spatial3D = spatial; }
    bool IsSpatial3D() const { return m_Spatial3D; }
    void SetMinDistance(float distance) { m_MinDistance = distance; }
    float GetMinDistance() const { return m_MinDistance; }
    void SetMaxDistance(float distance) { m_MaxDistance = distance; }
    float GetMaxDistance() const { return m_MaxDistance; }

    bool IsPlaying() const { return GetState() == SoundState::Playing; }
    SoundState GetState() const;
    double GetTime() const;
    float GetDuration() const;

    void OnAwake() override;
    void OnStart() override;
    void OnUpdate(float deltaTime) override;
    void OnDestroy() override;

private:
    IAudioBackend* GetBackend() const;

    std::shared_ptr<AudioClip> m_Clip;
    SoundId m_Source = kInvalidSoundId;
    bool m_Initialized = false;
    bool m_Playing = false;

    bool m_Looping = false;
    float m_Volume = 1.0f;
    float m_Pitch = 1.0f;
    bool m_PlayOnAwake = false;

    bool m_Spatial3D = false;
    float m_MinDistance = 1.0f;
    float m_MaxDistance = 500.0f;
};

} // namespace Leir