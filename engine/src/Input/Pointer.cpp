#include "LeirEngine/Input/Pointer.h"
#include "LeirEngine/Input/Mouse.h"
#include "LeirEngine/Input/Touch.h"
#include <algorithm>

namespace Leir {

struct PointerState {
    PointerButton current = PointerButton::None;
    PointerButton previous = PointerButton::None;
    glm::vec2 position{ 0.0f };
    glm::vec2 delta{ 0.0f };

    static PointerState& Get()
    {
        static PointerState state;
        return state;
    }
};

bool Pointer::IsDown(PointerButton btn)
{
    // Unified: true if any pointer source has this button
    if (Mouse::IsDown(btn)) return true;
    if (btn == PointerButton::Primary && Touch::IsDown(0)) return true;
    if (btn == PointerButton::Primary && Touch::GetCount() > 0) return true;
    return false;
}

bool Pointer::AreDown(PointerButton btns)
{
    auto bits = static_cast<uint16_t>(btns);
    uint16_t checked = 0;
    for (int i = 0; i < 11; ++i) {
        auto b = static_cast<PointerButton>(1 << i);
        if (bits & (1 << i)) {
            if (IsDown(b)) checked |= (1 << i);
        }
    }
    return checked == bits;
}

bool Pointer::IsUp(PointerButton btn)
{
    return !IsDown(btn);
}

bool Pointer::WasPressed(PointerButton btn)
{
    auto& state = PointerState::Get();
    return Has(state.current, btn) && !Has(state.previous, btn);
}

bool Pointer::WasReleased(PointerButton btn)
{
    auto& state = PointerState::Get();
    return !Has(state.current, btn) && Has(state.previous, btn);
}

float Pointer::GetX() { return PointerState::Get().position.x; }
float Pointer::GetY() { return PointerState::Get().position.y; }
glm::vec2 Pointer::GetPos() { return PointerState::Get().position; }
glm::vec2 Pointer::GetDelta() { return PointerState::Get().delta; }

void Pointer::ProcessEvent(const PointerEvent& e)
{
    auto& state = PointerState::Get();

    // Track button state
    auto btnBit = static_cast<uint16_t>(e.button);
    if (btnBit != 0) {
        if (e.action == EventAction::Press)
            state.current = static_cast<PointerButton>(static_cast<uint16_t>(state.current) | btnBit);
        else if (e.action == EventAction::Release)
            state.current = static_cast<PointerButton>(static_cast<uint16_t>(state.current) & ~btnBit);
    }

    // Position from most recent event
    state.position = e.position;
    state.delta = e.delta;
}

void Pointer::ResetFrame()
{
    auto& state = PointerState::Get();
    state.previous = state.current;
    state.delta = { 0.0f, 0.0f };
}

} // namespace Leir
