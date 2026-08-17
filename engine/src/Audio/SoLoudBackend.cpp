#include "SoLoudBackend.h"

#include "LeirEngine/Core/Log.h"

namespace Leir {

namespace {
constexpr SoLoud::result kSoloudOk = 0;
}

SoLoudBackend::SoLoudBackend() = default;

SoLoudBackend::~SoLoudBackend()
{
    Shutdown();
}

bool SoLoudBackend::Init()
{
    if (m_Initialized)
        return true;

    // Web: defer SoLoud::init() (which starts the MiniAudio->WebAudio device)
    // to WakeUp(), called from the first user gesture — the browser AudioContext
    // is born suspended and resume() only succeeds within a gesture. Desktop:
    // init right away (WASAPI device starts immediately).
#ifndef __EMSCRIPTEN__
    if (!InitDevice()) {
        XConsole::PrintError("[Audio] SoLoud init failed");
        return false;
    }
    m_Initialized = true;
    m_DeviceStarted = true;
    XConsole::Println("[Audio] SoLoud initialized (backend '{}')", m_Soloud.getBackendString());
#else
    m_Initialized = true;
    m_DeviceStarted = false;
#endif
    return true;
}

bool SoLoudBackend::InitDevice()
{
    unsigned int flags = SoLoud::Soloud::CLIP_ROUNDOFF;
#ifndef __EMSCRIPTEN__
    flags |= SoLoud::Soloud::ENABLE_VISUALIZATION;
#endif

    SoLoud::result res = m_Soloud.init(flags);
    if (res != kSoloudOk) {
        XConsole::PrintError("[Audio] SoLoud init failed: {}", m_Soloud.getErrorString(res));
        return false;
    }
    return true;
}

void SoLoudBackend::Shutdown()
{
    if (m_DeviceStarted) {
        for (auto& [id, rec] : m_Sources)
            if (rec.voice > 0)
                m_Soloud.stop(rec.voice);
        m_Sources.clear();
        m_Clips.clear();
        m_Soloud.deinit();
        m_DeviceStarted = false;
        XConsole::Println("[Audio] SoLoud shut down");
    }
    m_Initialized = false;
}

void SoLoudBackend::Update(float deltaTime)
{
    (void)deltaTime;
    if (m_DeviceStarted)
        m_Soloud.update3dAudio();
}

bool SoLoudBackend::WakeUp()
{
    if (!m_Initialized)
        return false;
    if (m_DeviceStarted)
        return true;
    // Web: first user gesture — create + start the WebAudio device now.
    if (!InitDevice())
        return false;
    m_DeviceStarted = true;
    XConsole::Println("[Audio] WebAudio device started (user gesture)");
    return true;
}

ClipId SoLoudBackend::LoadClip(const std::string& path)
{
    if (!m_Initialized)
        return 0;

    ClipId id = 1;
    while (m_Clips.find(id) != m_Clips.end())
        ++id;

    auto& [storedPath, wav] = m_Clips[id];
    storedPath = path;
    SoLoud::result res = wav.load(path.c_str());
    if (res != kSoloudOk) {
        XConsole::PrintError("[Audio] Failed to load '{}': {}", path, m_Soloud.getErrorString(res));
        m_Clips.erase(id);
        return 0;
    }
    return id;
}

void SoLoudBackend::UnloadClip(ClipId clip)
{
    if (clip != 0)
        m_Clips.erase(clip);
}

float SoLoudBackend::GetClipDuration(ClipId clip) const
{
    auto it = m_Clips.find(clip);
    if (it == m_Clips.end())
        return 0.0f;
    return static_cast<float>(it->second.second.getLength());
}

SoLoudBackend::SourceRec& SoLoudBackend::GetSource(SoundId source)
{
    auto it = m_Sources.find(source);
    if (it == m_Sources.end()) {
        SourceRec rec;
        rec.voice = 0;
        auto inserted = m_Sources.emplace(source, std::move(rec));
        return inserted.first->second;
    }
    return it->second;
}

SoundId SoLoudBackend::CreateSource()
{
    if (!m_Initialized)
        return kInvalidSoundId;
    SoundId id = m_NextSource++;
    while (m_Sources.find(id) != m_Sources.end())
        ++m_NextSource, id = m_NextSource;
    m_Sources[id] = SourceRec{};
    return id;
}

void SoLoudBackend::DestroySource(SoundId source)
{
    auto it = m_Sources.find(source);
    if (it == m_Sources.end())
        return;
    if (it->second.voice > 0 && m_DeviceStarted)
        m_Soloud.stop(it->second.voice);
    m_Sources.erase(it);
}

void SoLoudBackend::Play(SoundId source, ClipId clip, bool spatial3D, const Vector3& position)
{
    if (!m_DeviceStarted)
        return;

    auto clipIt = m_Clips.find(clip);
    if (clipIt == m_Clips.end()) {
        XConsole::PrintWarning("[Audio] Play: clip {} not loaded", clip);
        return;
    }

    SourceRec& rec = GetSource(source);
    if (rec.voice > 0)
        m_Soloud.stop(rec.voice);

    SoLoud::Wav& wav = clipIt->second.second;
    rec.clip = clip;
    rec.is3D = spatial3D;
    rec.position = position;

    if (spatial3D) {
        rec.voice = m_Soloud.play3d(wav, position.x, position.y, position.z,
                                    0.0f, 0.0f, 0.0f, rec.volume, 0);
    } else {
        rec.voice = m_Soloud.play(wav, rec.volume, 0.0f, 0);
    }

    if (rec.voice <= 0) {
        XConsole::PrintWarning("[Audio] Play: no voice available");
        return;
    }

    m_Soloud.setLooping(rec.voice, rec.looping);
    m_Soloud.setRelativePlaySpeed(rec.voice, rec.pitch);
    if (spatial3D)
        m_Soloud.set3dSourceMinMaxDistance(rec.voice, rec.minDistance, rec.maxDistance);
}

void SoLoudBackend::Stop(SoundId source)
{
    auto it = m_Sources.find(source);
    if (it != m_Sources.end() && it->second.voice > 0) {
        m_Soloud.stop(it->second.voice);
        it->second.voice = 0;
    }
}

void SoLoudBackend::Pause(SoundId source)
{
    auto it = m_Sources.find(source);
    if (it != m_Sources.end() && it->second.voice > 0)
        m_Soloud.setPause(it->second.voice, true);
}

void SoLoudBackend::Resume(SoundId source)
{
    auto it = m_Sources.find(source);
    if (it != m_Sources.end() && it->second.voice > 0)
        m_Soloud.setPause(it->second.voice, false);
}

void SoLoudBackend::Seek(SoundId source, double seconds)
{
    auto it = m_Sources.find(source);
    if (it != m_Sources.end() && it->second.voice > 0)
        m_Soloud.seek(it->second.voice, static_cast<float>(seconds));
}

void SoLoudBackend::SetLooping(SoundId source, bool loop)
{
    auto it = m_Sources.find(source);
    if (it == m_Sources.end())
        return;
    it->second.looping = loop;
    if (it->second.voice > 0)
        m_Soloud.setLooping(it->second.voice, loop);
}

void SoLoudBackend::SetVolume(SoundId source, float volume)
{
    auto it = m_Sources.find(source);
    if (it == m_Sources.end())
        return;
    it->second.volume = volume;
    if (it->second.voice > 0)
        m_Soloud.setVolume(it->second.voice, volume);
}

void SoLoudBackend::SetPitch(SoundId source, float pitch)
{
    auto it = m_Sources.find(source);
    if (it == m_Sources.end())
        return;
    it->second.pitch = pitch;
    if (it->second.voice > 0)
        m_Soloud.setRelativePlaySpeed(it->second.voice, pitch);
}

void SoLoudBackend::FadeVolume(SoundId source, float targetVolume, float seconds)
{
    auto it = m_Sources.find(source);
    if (it != m_Sources.end() && it->second.voice > 0)
        m_Soloud.fadeVolume(it->second.voice, targetVolume, seconds);
}

void SoLoudBackend::SetSource3D(SoundId source, const Vector3& position)
{
    auto it = m_Sources.find(source);
    if (it == m_Sources.end())
        return;
    it->second.is3D = true;
    it->second.position = position;
    if (it->second.voice > 0)
        m_Soloud.set3dSourcePosition(it->second.voice, position.x, position.y, position.z);
}

void SoLoudBackend::SetSource3DFalloff(SoundId source, float minDistance, float maxDistance)
{
    auto it = m_Sources.find(source);
    if (it == m_Sources.end())
        return;
    it->second.minDistance = minDistance;
    it->second.maxDistance = maxDistance;
    if (it->second.voice > 0 && it->second.is3D)
        m_Soloud.set3dSourceMinMaxDistance(it->second.voice, minDistance, maxDistance);
}

void SoLoudBackend::SetListener3D(const Vector3& position, const Vector3& forward, const Vector3& up)
{
    if (!m_DeviceStarted)
        return;
    m_Soloud.set3dListenerParameters(position.x, position.y, position.z,
                                     position.x + forward.x, position.y + forward.y, position.z + forward.z,
                                     up.x, up.y, up.z);
}

bool SoLoudBackend::Supports3D() const
{
    return true;
}

SoundState SoLoudBackend::GetState(SoundId source) const
{
    auto it = m_Sources.find(source);
    if (it == m_Sources.end() || !m_DeviceStarted)
        return SoundState::Stopped;
    const SourceRec& rec = it->second;
    if (rec.voice <= 0 || !m_Soloud.isValidVoiceHandle(rec.voice))
        return SoundState::Stopped;
    if (m_Soloud.getPause(rec.voice))
        return SoundState::Paused;
    return SoundState::Playing;
}

double SoLoudBackend::GetTime(SoundId source) const
{
    auto it = m_Sources.find(source);
    if (it == m_Sources.end() || !m_DeviceStarted)
        return 0.0;
    const SourceRec& rec = it->second;
    if (rec.voice <= 0 || !m_Soloud.isValidVoiceHandle(rec.voice))
        return 0.0;
    return static_cast<double>(m_Soloud.getStreamTime(rec.voice));
}

void SoLoudBackend::SetMasterVolume(float volume)
{
    if (m_DeviceStarted)
        m_Soloud.setGlobalVolume(volume);
}

float SoLoudBackend::GetMasterVolume() const
{
    return m_DeviceStarted ? m_Soloud.getGlobalVolume() : 1.0f;
}

} // namespace Leir