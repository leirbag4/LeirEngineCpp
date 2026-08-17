#pragma once

#include "LeirEngine/Audio/AudioTypes.h"
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector3.h"

#include <string>
#include <string_view>

namespace Leir {

// Static quick-play audio API (pattern: XConsole/Keyboard). Internally drives
// AudioEngine. All methods are callable anywhere; the audio is created on the
// first call (WakeUp is invoked automatically on web's first gesture attempt).
//
// Overload philosophy (C#-style): short methods for the common case, longer
// ones only when needed. Param name is always `volume`. A `Vector3` position
// makes the sound 3D; without it the sound is 2D. Music is a single active
// channel (playing another id switches it); its default loop is true.
//
// Ids: kInvalidSoundId (0) is reserved. SoundPlayer auto-assigns ids for the
// no-id Play(path) overloads from a high monotonic base (kAutoIdBase), so they
// never collide with small user ids.
class LEIR_API SoundPlayer {
public:
    // --- Play SFX ---
    static SoundId Play(std::string_view path);
    static SoundId Play(std::string_view path, const Vector3& position);
    static SoundId Play(SoundId id, std::string_view path);
    static SoundId Play(SoundId id, std::string_view path, const Vector3& position);
    static SoundId Play(SoundId id, bool loop, std::string_view path);
    static SoundId Play(SoundId id, bool loop, std::string_view path, const Vector3& position);
    static SoundId Play(SoundId id, bool loop, float volume, std::string_view path);
    static SoundId Play(SoundId id, bool loop, float volume, std::string_view path, const Vector3& position);
    static SoundId PlayOneShot(std::string_view path); // alias of Play(path)

    // --- Control by id ---
    static void Stop(SoundId id);
    static void Pause(SoundId id);
    static void Resume(SoundId id);
    static void Stop();   // the last no-id Play()
    static void Pause();  // the last no-id Play()
    static void Resume(); // the last no-id Play()
    static void Seek(SoundId id, double seconds);
    static void SetLoop(SoundId id, bool loop);
    static bool GetLoop(SoundId id);
    static void SetVolume(SoundId id, float volume);
    static float GetVolume(SoundId id);
    static void SetPitch(SoundId id, float pitch);
    static float GetPitch(SoundId id);
    static void SetPosition(SoundId id, const Vector3& position);
    static void FadeOut(SoundId id, float seconds);
    static void FadeTo(SoundId id, float volume, float seconds);

    // --- State ---
    static SoundState GetState(SoundId id);
    static bool IsPlaying(SoundId id);
    static double GetTime(SoundId id);
    static float GetDuration(SoundId id);
    // The last no-id quick-play that is still active, or kInvalidSoundId.
    static SoundId GetPlayingSound();

    // --- Music (default loop = true) ---
    static SoundId PlayMusic(SoundId id, std::string_view path); // loop = true
    static SoundId PlayMusic(SoundId id, bool loop, std::string_view path);
    static void StopMusic(SoundId id);
    static void StopMusic();
    static void PauseMusic(SoundId id);
    static void PauseMusic();
    static void ResumeMusic(SoundId id);
    static void ResumeMusic();
    static void SetMusicLoop(bool loop);
    static bool GetMusicLoop();
    static void SetMusicVolume(float volume);
    static float GetMusicVolume();
    static SoundState GetMusicState();
    static SoundId GetMusic();

    // --- Global ---
    static void SetMasterVolume(float volume);
    static float GetMasterVolume();
    static void StopAll(); // effects + music
    static void PauseAll();
    static void ResumeAll();
    static void FadeAll(float seconds);

private:
    static SoundId CreateSlot(SoundId id, bool isMusic);
};

} // namespace Leir