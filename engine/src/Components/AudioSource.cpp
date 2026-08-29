#include "LeirEngine/Components/AudioSource.h"

#include "LeirEngine/Audio/AudioBackend.h"
#include "LeirEngine/Audio/AudioClip.h"
#include "LeirEngine/Audio/AudioEngine.h"
#include "LeirEngine/Core/CoreObject.h"

namespace Leir {

AudioSource::AudioSource(AudioSource&& other) noexcept
    : Component(other)
    , m_Clip(std::move(other.m_Clip))
    , m_Source(other.m_Source)
    , m_Initialized(other.m_Initialized)
    , m_Playing(other.m_Playing)
    , m_Started(other.m_Started)
    , m_Looping(other.m_Looping)
    , m_Volume(other.m_Volume)
    , m_Pitch(other.m_Pitch)
    , m_PlayOnAwake(other.m_PlayOnAwake)
    , m_Spatial3D(other.m_Spatial3D)
    , m_MinDistance(other.m_MinDistance)
    , m_MaxDistance(other.m_MaxDistance)
{
    other.m_Source = kInvalidSoundId;
    other.m_Initialized = false;
}

AudioSource& AudioSource::operator=(AudioSource&& other) noexcept
{
    if (this == &other)
        return *this;
    FreeSource();
    m_Clip = std::move(other.m_Clip);
    m_Source = other.m_Source;
    m_Initialized = other.m_Initialized;
    m_Playing = other.m_Playing;
    m_Started = other.m_Started;
    m_Looping = other.m_Looping;
    m_Volume = other.m_Volume;
    m_Pitch = other.m_Pitch;
    m_PlayOnAwake = other.m_PlayOnAwake;
    m_Spatial3D = other.m_Spatial3D;
    m_MinDistance = other.m_MinDistance;
    m_MaxDistance = other.m_MaxDistance;
    other.m_Source = kInvalidSoundId;
    other.m_Initialized = false;
    return *this;
}

AudioSource::~AudioSource()
{
    FreeSource();
}

void AudioSource::FreeSource()
{
    if (m_Initialized) {
        if (IAudioBackend* backend = AudioEngine::GetInstance().GetBackend())
            backend->DestroySource(m_Source);
        m_Initialized = false;
        m_Source = kInvalidSoundId;
    }
}

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

void AudioSource::AutoStart()
{
    if (m_Started)
        return;
    m_Started = true;
    if (m_PlayOnAwake)
        Play();
}

void AudioSource::Sync3D(const Vector3& pos)
{
    if (m_Initialized && m_Spatial3D) {
        if (IAudioBackend* backend = GetBackend())
            backend->SetSource3D(m_Source, pos);
    }
}

IAudioBackend* AudioSource::GetBackend() const
{
    return AudioEngine::GetInstance().GetBackend();
}

} // namespace Leir