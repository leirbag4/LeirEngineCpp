#pragma once
#include "LeirEngine/Input/Key.h"
#include "LeirEngine/Input/PointerButton.h"
#include <glm/glm.hpp>
#include <variant>
#include <cstdint>

namespace Leir {

enum class EventAction : uint8_t {
    Press,
    Release,
    Repeat,
    Move,
    Cancel,
};

enum class PointerSource : uint8_t {
    Mouse,
    Touch,
    Pen,
};

struct KeyEvent {
    Key key = Key::Unknown;
    int scancode = 0;
    EventAction action = EventAction::Press;
    int mods = 0;
};

struct PointerEvent {
    PointerSource source = PointerSource::Mouse;
    int pointerId = 0;
    glm::vec2 position{ 0.0f };
    glm::vec2 delta{ 0.0f };
    PointerButton button = PointerButton::None;
    EventAction action = EventAction::Press;
    float pressure = 1.0f;
};

struct CharEvent {
    uint32_t codepoint = 0;
    int mods = 0;
};

struct ScrollEvent {
    glm::vec2 offset{ 0.0f };
};

using InputEvent = std::variant<KeyEvent, PointerEvent, CharEvent, ScrollEvent>;

} // namespace Leir
