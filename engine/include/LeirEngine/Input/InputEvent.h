#pragma once
#include "LeirEngine/Input/Key.h"
#include "LeirEngine/Input/PointerButton.h"
#include "LeirEngine/Math/Vector2.h"
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
    void* window = nullptr; ///< GLFWwindow* origin (nullptr = single-window apps).
};

struct PointerEvent {
    PointerSource source = PointerSource::Mouse;
    int pointerId = 0;
    Vector2 position{ 0.0f, 0.0f };
    Vector2 delta{ 0.0f, 0.0f };
    PointerButton button = PointerButton::None;
    EventAction action = EventAction::Press;
    float pressure = 1.0f;
    void* window = nullptr; ///< GLFWwindow* origin (nullptr = single-window apps).
};

struct CharEvent {
    uint32_t codepoint = 0;
    int mods = 0;
    void* window = nullptr; ///< GLFWwindow* origin (nullptr = single-window apps).
};

struct ScrollEvent {
    Vector2 offset{ 0.0f, 0.0f };
    void* window = nullptr; ///< GLFWwindow* origin (nullptr = single-window apps).
};

using InputEvent = std::variant<KeyEvent, PointerEvent, CharEvent, ScrollEvent>;

} // namespace Leir
