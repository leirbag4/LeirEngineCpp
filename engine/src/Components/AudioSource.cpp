#include "LeirEngine/Components/AudioSource.h"

#include "LeirEngine/Audio/AudioBackend.h"
#include "LeirEngine/Audio/AudioClip.h"
#include "LeirEngine/Audio/AudioEngine.h"
#include "LeirEngine/Core/CoreObject.h"

namespace Leir {

void AudioSource::SetClip(std::shared_ptr<AudioClip> clip)
{
    m_Clip = std::move(clip);
}

void AudioSource::SetClipPath(const std::string& path)
{
    m_Clip = AudioClip::Load(path);
}

void AudioSource::Play()
{
    if (!m_Clip || !m_Initialized)
        return;
    IAudioBackend* backend = GetBackend();
    if (!backend)
        return;

    Vector3 pos = GetOwner()->GetTransform().GetWorldPosition();
    backend->Play(m_Source, m_Clip->GetClipId(), m_Spatial3D, pos);
    backend->SetLooping(m_Source, m_Looping);
    backend->SetVolume(m_Source, m_Volume);
    backend->SetPitch(m_Source, m_Pitch);
    if (m_Spatial3D)
        backend->SetSource3DFalloff(m_Source, m_MinDistance, m_MaxDistance);
    m_Playing = true;
}

void AudioSource::Stop()
{
    if (!m_Initialized)
        return;
    if (IAudioBackend* backend = GetBackend())
        backend->Stop(m_Source);
    m_Playing = false;
}

void AudioSource::Pause()
{
    if (m_Initialized)
        if (IAudioBackend* backend = GetBackend())
            backend->Pause(m_Source);
}

void AudioSource::Resume()
{
    if (m_Initialized)
        if (IAudioBackend* backend = GetBackend())
            backend->Resume(m_Source);
}

void AudioSource::Seek(double seconds)
{
    if (m_Initialized)
        if (IAudioBackend* backend = GetBackend())
            backend->Seek(m_Source, seconds);
}

SoundState AudioSource::GetState() const
{
    if (!m_Initialized)
        return SoundState::Stopped;
    IAudioBackend* backend = AudioEngine::GetInstance().GetBackend();
    if (!backend)
        return SoundState::Stopped;
    return backend->GetState(m_Source);
}

double AudioSource::GetTime() const
{
    if (!m_Initialized)
        return 0.0;
    IAudioBackend* backend = AudioEngine::GetInstance().GetBackend();
    return backend ? backend->GetTime(m_Source) : 0.0;
}

float AudioSource::GetDuration() const
{
    return m_Clip ? m_Clip->GetDuration() : 0.0f;
}

void AudioSource::OnAwake()
{
    m_Initialized = false;
    IAudioBackend* backend = AudioEngine::GetInstance().GetBackend();
    if (backend) {
        m_Source = backend->CreateSource();
        m_Initialized = (m_Source != kInvalidSoundId);
    }
}

void AudioSource::OnStart()
{
    if (m_PlayOnAwake)
        Play();
}

void AudioSource::OnUpdate(float deltaTime)
{
    (void)deltaTime;
    if (m_Initialized && m_Spatial3D) {
        if (IAudioBackend* backend = GetBackend()) {
            Vector3 pos = GetOwner()->GetTransform().GetWorldPosition();
            backend->SetSource3D(m_Source, pos);
        }
    }
}

void AudioSource::OnDestroy()
{
    if (m_Initialized) {
        if (IAudioBackend* backend = AudioEngine::GetInstance().GetBackend())
            backend->DestroySource(m_Source);
        m_Initialized = false;
        m_Source = kInvalidSoundId;
    }
}

IAudioBackend* AudioSource::GetBackend() const
{
    return AudioEngine::GetInstance().GetBackend();
}

} // namespace Leir