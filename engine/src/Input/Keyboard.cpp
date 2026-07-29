#include "LeirEngine/Input/Keyboard.h"
#include <string>

namespace Leir {

struct KeyboardState {
    bool current[384] = {};
    bool previous[384] = {};
    Key lastPressedKey = Key::Unknown;

    static KeyboardState& Get()
    {
        static KeyboardState state;
        return state;
    }
};

bool Keyboard::IsDown(Key key)
{
    auto k = static_cast<int32_t>(key);
    if (k < 0 || k >= 384) return false;
    return KeyboardState::Get().current[k];
}

bool Keyboard::IsDown(const KeyCombo& combo)
{
    for (auto k : combo.keys) {
        if (!IsDown(k)) return false;
    }
    return !combo.keys.empty();
}

bool Keyboard::IsUp(Key key)
{
    return !IsDown(key);
}

bool Keyboard::WasPressed(Key key)
{
    auto k = static_cast<int32_t>(key);
    if (k < 0 || k >= 384) return false;
    auto& state = KeyboardState::Get();
    return state.current[k] && !state.previous[k];
}

bool Keyboard::WasReleased(Key key)
{
    auto k = static_cast<int32_t>(key);
    if (k < 0 || k >= 384) return false;
    auto& state = KeyboardState::Get();
    return !state.current[k] && state.previous[k];
}

Key Keyboard::GetLastPressedKey()
{
    return KeyboardState::Get().lastPressedKey;
}

std::string Keyboard::GetPressedKeysString()
{
    auto& state = KeyboardState::Get();
    std::string result;
    for (int i = 0; i < 384; ++i) {
        if (state.current[i]) {
            if (!result.empty()) result += ", ";
            switch (static_cast<Key>(i)) {
                case Key::Space:        result += "Space"; break;
                case Key::Enter:        result += "Enter"; break;
                case Key::Tab:          result += "Tab"; break;
                case Key::Backspace:    result += "Backspace"; break;
                case Key::Escape:       result += "Escape"; break;
                case Key::LeftShift:    result += "LShift"; break;
                case Key::RightShift:   result += "RShift"; break;
                case Key::LeftControl:  result += "LCtrl"; break;
                case Key::RightControl: result += "RCtrl"; break;
                case Key::LeftAlt:      result += "LAlt"; break;
                case Key::RightAlt:     result += "RAlt"; break;
                case Key::LeftSuper:    result += "LSuper"; break;
                case Key::RightSuper:   result += "RSuper"; break;
                case Key::Up:           result += "Up"; break;
                case Key::Down:         result += "Down"; break;
                case Key::Left:         result += "Left"; break;
                case Key::Right:        result += "Right"; break;
                case Key::F1:  case Key::F2:  case Key::F3:
                case Key::F4:  case Key::F5:  case Key::F6:
                case Key::F7:  case Key::F8:  case Key::F9:
                case Key::F10: case Key::F11: case Key::F12:
                    result += "F" + std::to_string(i - 289);
                    break;
                default:
                    if (i >= 65 && i <= 90) {
                        result += static_cast<char>(i);
                    } else if (i >= 48 && i <= 57) {
                        result += static_cast<char>(i);
                    } else {
                        result += "[" + std::to_string(i) + "]";
                    }
                    break;
            }
        }
    }
    return result;
}

void Keyboard::ProcessEvent(const KeyEvent& e)
{
    auto k = static_cast<int32_t>(e.key);
    if (k < 0 || k >= 384) return;
    auto& state = KeyboardState::Get();
    state.current[k] = (e.action == EventAction::Press || e.action == EventAction::Repeat);
    if (e.action == EventAction::Press)
        state.lastPressedKey = e.key;
}

void Keyboard::ResetFrame()
{
    auto& state = KeyboardState::Get();
    for (int i = 0; i < 384; ++i)
        state.previous[i] = state.current[i];
}

} // namespace Leir
