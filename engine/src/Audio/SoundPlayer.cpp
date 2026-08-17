#include "LeirEngine/Audio/SoundPlayer.h"

#include "LeirEngine/Audio/AudioBackend.h"
#include "LeirEngine/Audio/AudioEngine.h"

#include <unordered_map>

namespace Leir {

namespace {

// Slots for SoundId space: which backend source, which clip, what settings.
// Lives in function-local statics (pattern: XConsole) — no static-init order.
struct SoundSlot {
    SoundId backendId = kInvalidSoundId;
    ClipId clip = 0;
    std::string path;
    bool isMusic = false;
    bool loop = false;
    float volume = 1.0f;
    float pitch = 1.0f;
};

constexpr SoundId kAutoIdBase = 0x10000000u;

std::unordered_map<SoundId, SoundSlot>& Slots()
{
    static std::unordered_map<SoundId, SoundSlot> slots;
    return slots;
}

SoundId& NextAutoId()
{
    static SoundId next = kAutoIdBase;
    return next;
}

SoundId& LastQuick()
{
    static SoundId last = kInvalidSoundId;
    return last;
}

SoundId& Music()
{
    static SoundId music = kInvalidSoundId;
    return music;
}

IAudioBackend* EnsureAudio()
{
    AudioEngine& engine = AudioEngine::GetInstance();
    if (!engine.IsInitialized())
        engine.Init();
    engine.WakeUp();
    return engine.GetBackend();
}

SoundSlot& GetOrCreateSlot(SoundId id, bool isMusic)
{
    auto& slots = Slots();
    auto it = slots.find(id);
    if (it != slots.end()) {
        it->second.isMusic = isMusic;
        return it->second;
    }
    SoundSlot slot;
    IAudioBackend* backend = EnsureAudio();
    slot.backendId = backend ? backend->CreateSource() : kInvalidSoundId;
    slot.isMusic = isMusic;
    auto inserted = slots.emplace(id, std::move(slot));
    return inserted.first->second;
}

SoundSlot* FindSlot(SoundId id)
{
    auto& slots = Slots();
    auto it = slots.find(id);
    return it != slots.end() ? &it->second : nullptr;
}

SoundId PlayImpl(SoundId id, bool loop, float volume, std::string_view path, const Vector3* position, bool isMusic)
{
    IAudioBackend* backend = EnsureAudio();
    if (!backend)
        return kInvalidSoundId;

    AudioEngine& engine = AudioEngine::GetInstance();
    ClipId clipId = engine.GetOrLoadClip(std::string(path));
    if (clipId == 0)
        return kInvalidSoundId;

    if (isMusic) {
        // Music is a single active channel: stop the previous one.
        SoundId current = Music();
        if (current != kInvalidSoundId && current != id) {
            SoundSlot* prev = FindSlot(current);
            if (prev && prev->isMusic)
                backend->Stop(prev->backendId);
        }
    }

    if (id == kInvalidSoundId) {
        id = LastQuick();
        if (id == kInvalidSoundId || !FindSlot(id)) {
            id = NextAutoId();
            while (Slots().count(id))
                id = NextAutoId();
            LastQuick() = id;
        }
    }

    SoundSlot& slot = GetOrCreateSlot(id, isMusic);
    if (slot.backendId == kInvalidSoundId)
        return kInvalidSoundId;

    slot.path = std::string(path);
    slot.clip = clipId;
    slot.loop = loop;
    slot.volume = volume;

    bool spatial = position != nullptr;
    backend->Play(slot.backendId, clipId, spatial, position ? *position : Vector3(0.0f, 0.0f, 0.0f));
    backend->SetLooping(slot.backendId, loop);
    backend->SetVolume(slot.backendId, volume);

    if (isMusic)
        Music() = id;

    return id;
}

} // namespace

// --- Play SFX ---

SoundId SoundPlayer::Play(std::string_view path)
{
    return PlayImpl(kInvalidSoundId, false, 1.0f, path, nullptr, false);
}

SoundId SoundPlayer::Play(std::string_view path, const Vector3& position)
{
    return PlayImpl(kInvalidSoundId, false, 1.0f, path, &position, false);
}

SoundId SoundPlayer::Play(SoundId id, std::string_view path)
{
    return PlayImpl(id, false, 1.0f, path, nullptr, false);
}

SoundId SoundPlayer::Play(SoundId id, std::string_view path, const Vector3& position)
{
    return PlayImpl(id, false, 1.0f, path, &position, false);
}

SoundId SoundPlayer::Play(SoundId id, bool loop, std::string_view path)
{
    return PlayImpl(id, loop, 1.0f, path, nullptr, false);
}

SoundId SoundPlayer::Play(SoundId id, bool loop, std::string_view path, const Vector3& position)
{
    return PlayImpl(id, loop, 1.0f, path, &position, false);
}

SoundId SoundPlayer::Play(SoundId id, bool loop, float volume, std::string_view path)
{
    return PlayImpl(id, loop, volume, path, nullptr, false);
}

SoundId SoundPlayer::Play(SoundId id, bool loop, float volume, std::string_view path, const Vector3& position)
{
    return PlayImpl(id, loop, volume, path, &position, false);
}

SoundId SoundPlayer::PlayOneShot(std::string_view path)
{
    return Play(path);
}

// --- Control by id ---

void SoundPlayer::Stop(SoundId id)
{
    SoundSlot* slot = FindSlot(id);
    IAudioBackend* backend = EnsureAudio();
    if (slot && backend)
        backend->Stop(slot->backendId);
}

void SoundPlayer::Pause(SoundId id)
{
    SoundSlot* slot = FindSlot(id);
    IAudioBackend* backend = EnsureAudio();
    if (slot && backend)
        backend->Pause(slot->backendId);
}

void SoundPlayer::Resume(SoundId id)
{
    SoundSlot* slot = FindSlot(id);
    IAudioBackend* backend = EnsureAudio();
    if (slot && backend)
        backend->Resume(slot->backendId);
}

void SoundPlayer::Stop() { if (LastQuick() != kInvalidSoundId) Stop(LastQuick()); }
void SoundPlayer::Pause() { if (LastQuick() != kInvalidSoundId) Pause(LastQuick()); }
void SoundPlayer::Resume() { if (LastQuick() != kInvalidSoundId) Resume(LastQuick()); }

void SoundPlayer::Seek(SoundId id, double seconds)
{
    SoundSlot* slot = FindSlot(id);
    IAudioBackend* backend = EnsureAudio();
    if (slot && backend)
        backend->Seek(slot->backendId, seconds);
}

void SoundPlayer::SetLoop(SoundId id, bool loop)
{
    SoundSlot* slot = FindSlot(id);
    IAudioBackend* backend = EnsureAudio();
    if (!slot || !backend)
        return;
    slot->loop = loop;
    backend->SetLooping(slot->backendId, loop);
}

bool SoundPlayer::GetLoop(SoundId id)
{
    SoundSlot* slot = FindSlot(id);
    return slot ? slot->loop : false;
}

void SoundPlayer::SetVolume(SoundId id, float volume)
{
    SoundSlot* slot = FindSlot(id);
    IAudioBackend* backend = EnsureAudio();
    if (!slot || !backend)
        return;
    slot->volume = volume;
    backend->SetVolume(slot->backendId, volume);
}

float SoundPlayer::GetVolume(SoundId id)
{
    SoundSlot* slot = FindSlot(id);
    return slot ? slot->volume : 0.0f;
}

void SoundPlayer::SetPitch(SoundId id, float pitch)
{
    SoundSlot* slot = FindSlot(id);
    IAudioBackend* backend = EnsureAudio();
    if (!slot || !backend)
        return;
    slot->pitch = pitch;
    backend->SetPitch(slot->backendId, pitch);
}

float SoundPlayer::GetPitch(SoundId id)
{
    SoundSlot* slot = FindSlot(id);
    return slot ? slot->pitch : 1.0f;
}

void SoundPlayer::SetPosition(SoundId id, const Vector3& position)
{
    SoundSlot* slot = FindSlot(id);
    IAudioBackend* backend = EnsureAudio();
    if (slot && backend)
        backend->SetSource3D(slot->backendId, position);
}

void SoundPlayer::FadeOut(SoundId id, float seconds)
{
    SoundSlot* slot = FindSlot(id);
    IAudioBackend* backend = EnsureAudio();
    if (slot && backend)
        backend->FadeVolume(slot->backendId, 0.0f, seconds);
}

void SoundPlayer::FadeTo(SoundId id, float volume, float seconds)
{
    SoundSlot* slot = FindSlot(id);
    IAudioBackend* backend = EnsureAudio();
    if (slot && backend)
        backend->FadeVolume(slot->backendId, volume, seconds);
}

// --- State ---

SoundState SoundPlayer::GetState(SoundId id)
{
    SoundSlot* slot = FindSlot(id);
    IAudioBackend* backend = EnsureAudio();
    if (!slot || !backend)
        return SoundState::Stopped;
    return backend->GetState(slot->backendId);
}

bool SoundPlayer::IsPlaying(SoundId id)
{
    return GetState(id) == SoundState::Playing;
}

double SoundPlayer::GetTime(SoundId id)
{
    SoundSlot* slot = FindSlot(id);
    IAudioBackend* backend = EnsureAudio();
    if (!slot || !backend)
        return 0.0;
    return backend->GetTime(slot->backendId);
}

float SoundPlayer::GetDuration(SoundId id)
{
    SoundSlot* slot = FindSlot(id);
    IAudioBackend* backend = EnsureAudio();
    if (!slot || !backend || slot->clip == 0)
        return 0.0f;
    return backend->GetClipDuration(slot->clip);
}

SoundId SoundPlayer::GetPlayingSound()
{
    SoundId id = LastQuick();
    if (id == kInvalidSoundId)
        return kInvalidSoundId;
    SoundState state = GetState(id);
    return state == SoundState::Stopped ? kInvalidSoundId : id;
}

// --- Music ---

SoundId SoundPlayer::PlayMusic(SoundId id, std::string_view path)
{
    return PlayImpl(id, true, 1.0f, path, nullptr, true);
}

SoundId SoundPlayer::PlayMusic(SoundId id, bool loop, std::string_view path)
{
    return PlayImpl(id, loop, 1.0f, path, nullptr, true);
}

void SoundPlayer::StopMusic(SoundId id)
{
    Stop(id);
    if (Music() == id)
        Music() = kInvalidSoundId;
}

void SoundPlayer::StopMusic() { if (Music() != kInvalidSoundId) StopMusic(Music()); }

void SoundPlayer::PauseMusic(SoundId id) { Pause(id); }
void SoundPlayer::PauseMusic() { if (Music() != kInvalidSoundId) PauseMusic(Music()); }

void SoundPlayer::ResumeMusic(SoundId id) { Resume(id); }
void SoundPlayer::ResumeMusic() { if (Music() != kInvalidSoundId) ResumeMusic(Music()); }

void SoundPlayer::SetMusicLoop(bool loop)
{
    SoundId id = Music();
    if (id != kInvalidSoundId)
        SetLoop(id, loop);
}

bool SoundPlayer::GetMusicLoop()
{
    SoundId id = Music();
    return id != kInvalidSoundId && GetLoop(id);
}

void SoundPlayer::SetMusicVolume(float volume)
{
    SoundId id = Music();
    if (id != kInvalidSoundId)
        SetVolume(id, volume);
}

float SoundPlayer::GetMusicVolume()
{
    SoundId id = Music();
    return id != kInvalidSoundId ? GetVolume(id) : 0.0f;
}

SoundState SoundPlayer::GetMusicState()
{
    SoundId id = Music();
    return id != kInvalidSoundId ? GetState(id) : SoundState::Stopped;
}

SoundId SoundPlayer::GetMusic()
{
    return Music();
}

// --- Global ---

void SoundPlayer::SetMasterVolume(float volume)
{
    IAudioBackend* backend = EnsureAudio();
    if (backend)
        backend->SetMasterVolume(volume);
}

float SoundPlayer::GetMasterVolume()
{
    IAudioBackend* backend = EnsureAudio();
    return backend ? backend->GetMasterVolume() : 1.0f;
}

void SoundPlayer::StopAll()
{
    SoundId music = Music();
    if (music != kInvalidSoundId)
        Stop(music);
    IAudioBackend* backend = EnsureAudio();
    for (auto& [id, slot] : Slots()) {
        if (!slot.isMusic && backend)
            backend->Stop(slot.backendId);
    }
}

void SoundPlayer::PauseAll()
{
    IAudioBackend* backend = EnsureAudio();
    for (auto& [id, slot] : Slots())
        if (backend)
            backend->Pause(slot.backendId);
}

void SoundPlayer::ResumeAll()
{
    IAudioBackend* backend = EnsureAudio();
    for (auto& [id, slot] : Slots())
        if (backend)
            backend->Resume(slot.backendId);
}

void SoundPlayer::FadeAll(float seconds)
{
    IAudioBackend* backend = EnsureAudio();
    for (auto& [id, slot] : Slots())
        if (backend)
            backend->FadeVolume(slot.backendId, 0.0f, seconds);
}

} // namespace Leir