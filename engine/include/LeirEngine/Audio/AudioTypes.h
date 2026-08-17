#pragma once

#include "LeirEngine/Core/Export.h"

#include <cstdint>

namespace Leir {

// Opaque handle to a playable source (voice). Returned by SoundPlayer::Play /
// AudioEngine sources; ids >= kAutoIdBase are auto-assigned by SoundPlayer.
using SoundId = uint32_t;
inline constexpr SoundId kInvalidSoundId = 0;

// Opaque handle to a decoded clip in the audio backend.
using ClipId = uint32_t;

// Playback state of a source.
enum class SoundState : uint8_t {
    Stopped = 0, // not playing
    Playing = 1,
    Paused = 2,
    Loading = 3, // clip still decoding
};

} // namespace Leir