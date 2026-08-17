#pragma once

#include "LeirEngine/Audio/AudioTypes.h"
#include "LeirEngine/Core/Export.h"

#include <memory>
#include <string>

namespace Leir {

// Decoded audio asset (path + duration + backend clip handle). Loaded through
// AudioEngine (which caches clips by path, so the same file is decoded once).
class LEIR_API AudioClip {
public:
    // Loads (or returns a cached) clip. Returns nullptr if the file can't load.
    static std::shared_ptr<AudioClip> Load(const std::string& path);

    // Creates a clip bound to an already-loaded backend clip (used by
    // AudioEngine). Returns nullptr when the backend has no such clip.
    static std::shared_ptr<AudioClip> Create(const std::string& path, float duration, ClipId clipId);

    const std::string& GetPath() const { return m_Path; }
    float GetDuration() const { return m_Duration; } // seconds
    ClipId GetClipId() const { return m_ClipId; }

private:
    AudioClip() = default;
    AudioClip(const AudioClip&) = delete;
    AudioClip& operator=(const AudioClip&) = delete;

    std::string m_Path;
    float m_Duration = 0.0f;
    ClipId m_ClipId = 0;
};

} // namespace Leir