#include "LeirEngine/Input/Keyboard.h"

namespace Leir {

KeyboardState& KeyboardState::Get()
{
    static KeyboardState state;
    return state;
}

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

void Keyboard::ProcessEvent(const KeyEvent& e)
{
    auto k = static_cast<int32_t>(e.key);
    if (k < 0 || k >= 384) return;
    auto& state = KeyboardState::Get();
    state.current[k] = (e.action == EventAction::Press || e.action == EventAction::Repeat);
}

void Keyboard::ResetFrame()
{
    auto& state = KeyboardState::Get();
    for (int i = 0; i < 384; ++i)
        state.previous[i] = state.current[i];
}

} // namespace Leir
