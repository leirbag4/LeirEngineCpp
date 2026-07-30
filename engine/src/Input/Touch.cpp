#include "LeirEngine/Input/Touch.h"

namespace Leir {

struct TouchState {
    std::vector<TouchFinger> fingers;
    std::vector<TouchFinger> prevFingers;

    static TouchState& Get()
    {
        static TouchState state;
        return state;
    }
};

const std::vector<TouchFinger>& Touch::GetFingers()
{
    return TouchState::Get().fingers;
}

int Touch::GetCount()
{
    int count = 0;
    for (const auto& f : TouchState::Get().fingers) {
        if (f.down) ++count;
    }
    return count;
}

bool Touch::IsDown(int fingerId)
{
    auto& state = TouchState::Get();
    for (const auto& f : state.fingers) {
        if (f.id == fingerId) return f.down;
    }
    return false;
}

bool Touch::IsUp(int fingerId)
{
    return !IsDown(fingerId);
}

bool Touch::WasPressed(int fingerId)
{
    auto& state = TouchState::Get();
    for (const auto& f : state.fingers) {
        if (f.id == fingerId) return f.pressed;
    }
    return false;
}

bool Touch::WasReleased(int fingerId)
{
    auto& state = TouchState::Get();
    for (const auto& f : state.fingers) {
        if (f.id == fingerId) return f.released;
    }
    return false;
}

void Touch::ProcessEvent(const PointerEvent& e)
{
    if (e.source != PointerSource::Touch) return;
    auto& state = TouchState::Get();

    // Find or create finger
    TouchFinger* finger = nullptr;
    for (auto& f : state.fingers) {
        if (f.id == e.pointerId) {
            finger = &f;
            break;
        }
    }
    if (!finger) {
        TouchFinger nf;
        nf.id = e.pointerId;
        state.fingers.push_back(nf);
        finger = &state.fingers.back();
    }

    if (e.action == EventAction::Press) {
        finger->down = true;
        finger->pressed = true;
        finger->released = false;
    } else if (e.action == EventAction::Release) {
        finger->down = false;
        finger->pressed = false;
        finger->released = true;
    } else if (e.action == EventAction::Move) {
        // move, no edge change
    } else if (e.action == EventAction::Cancel) {
        finger->down = false;
        finger->pressed = false;
        finger->released = true;
    }

    finger->position = e.position;
    finger->delta = e.delta;
    finger->pressure = e.pressure;
}

void Touch::ResetFrame()
{
    auto& state = TouchState::Get();
    for (auto& f : state.fingers) {
        f.pressed = false;
        f.released = false;
        f.delta = Vector2{ 0.0f, 0.0f };
    }
}

} // namespace Leir
