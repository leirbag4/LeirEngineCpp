#include "LeirEngine/Input/Mouse.h"

namespace Leir {

struct MouseState {
    bool current[16] = {};
    bool previous[16] = {};
    glm::vec2 position{ 0.0f };
    glm::vec2 delta{ 0.0f };
    float scrollDelta = 0.0f;

    static MouseState& Get()
    {
        static MouseState state;
        return state;
    }
};

bool Mouse::IsDown(PointerButton btn)
{
    auto b = static_cast<uint16_t>(btn);
    if (b >= 16) return false;
    return MouseState::Get().current[b];
}

bool Mouse::IsUp(PointerButton btn)
{
    return !IsDown(btn);
}

bool Mouse::WasPressed(PointerButton btn)
{
    auto b = static_cast<uint16_t>(btn);
    if (b >= 16) return false;
    auto& state = MouseState::Get();
    return state.current[b] && !state.previous[b];
}

bool Mouse::WasReleased(PointerButton btn)
{
    auto b = static_cast<uint16_t>(btn);
    if (b >= 16) return false;
    auto& state = MouseState::Get();
    return !state.current[b] && state.previous[b];
}

float Mouse::GetX() { return MouseState::Get().position.x; }
float Mouse::GetY() { return MouseState::Get().position.y; }
glm::vec2 Mouse::GetPos() { return MouseState::Get().position; }
glm::vec2 Mouse::GetDelta() { return MouseState::Get().delta; }
float Mouse::GetScrollDelta() { return MouseState::Get().scrollDelta; }

void Mouse::ProcessEvent(const PointerEvent& e)
{
    if (e.source != PointerSource::Mouse) return;
    auto& state = MouseState::Get();
    auto b = static_cast<uint16_t>(e.button);
    if (b < 16) {
        if (e.action == EventAction::Press)
            state.current[b] = true;
        else if (e.action == EventAction::Release)
            state.current[b] = false;
    }
    state.position = e.position;
    state.delta = e.delta;
}

void Mouse::ProcessScroll(const ScrollEvent& e)
{
    MouseState::Get().scrollDelta = e.offset.y;
}

void Mouse::ResetFrame()
{
    auto& state = MouseState::Get();
    for (int i = 0; i < 16; ++i)
        state.previous[i] = state.current[i];
    state.delta = { 0.0f, 0.0f };
    state.scrollDelta = 0.0f;
}

} // namespace Leir
