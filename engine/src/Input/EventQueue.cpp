#include "LeirEngine/Input/EventQueue.h"
#include "LeirEngine/Input/Keyboard.h"
#include "LeirEngine/Input/Mouse.h"
#include "LeirEngine/Input/Pointer.h"
#include "LeirEngine/Input/Touch.h"
#include "LeirEngine/Input/InputManager.h"

namespace Leir {

EventQueue& EventQueue::Get()
{
    static EventQueue instance;
    return instance;
}

void EventQueue::Push(const InputEvent& event)
{
    std::lock_guard lock(m_Mutex);
    m_Queue.push_back(event);
}

// The global polling state (Keyboard/Mouse/Touch/Pointer) reflects the physical
// input devices, so events from ALL windows update it — a key press or mouse
// move is a device-level fact regardless of which window delivered it. Each
// UICanvas already filters events by its own window in its hooks, so the global
// state only feeds polling queries (Mouse::GetDelta, Keyboard::IsDown, ...);
// the editor gates camera/gizmo/picking on the viewport hover, which keeps the
// coordinates consistent (the hover comes from the same window's events).
void EventQueue::Process()
{
    std::vector<InputEvent> events;
    {
        std::lock_guard lock(m_Mutex);
        events.swap(m_Queue);
    }

    // Iterate SNAPSHOTS of the hook lists, not the live vectors. A hook's body
    // may register/remove hooks (e.g. creating or closing an external window
    // inside a UI callback — the About window, a detached dock panel), which
    // would reallocate/erase the vector while this range-for is iterating it →
    // iterator invalidation → use-after-free → 0xC0000005. Copying the (tiny)
    // lists makes adds/removes during dispatch take effect next frame, which is
    // the standard observer-pattern behavior.
    for (const auto& event : events) {
        std::visit([this](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, KeyEvent>) {
                Keyboard::ProcessEvent(e);
                auto hooks = m_KeyHooks;
                for (auto& [id, h] : hooks) (void)id, h(e);
            } else if constexpr (std::is_same_v<T, PointerEvent>) {
                Mouse::ProcessEvent(e);
                Touch::ProcessEvent(e);
                Pointer::ProcessEvent(e);
                auto hooks = m_PointerHooks;
                for (auto& [id, h] : hooks) (void)id, h(e);
            } else if constexpr (std::is_same_v<T, CharEvent>) {
                auto hooks = m_CharHooks;
                for (auto& [id, h] : hooks) (void)id, h(e);
            } else if constexpr (std::is_same_v<T, ScrollEvent>) {
                Mouse::ProcessScroll(e);
                auto hooks = m_ScrollHooks;
                for (auto& [id, h] : hooks) (void)id, h(e);
            }
        }, event);
    }
}

void EventQueue::ClearHooks()
{
    m_KeyHooks.clear();
    m_CharHooks.clear();
    m_PointerHooks.clear();
    m_ScrollHooks.clear();
}

} // namespace Leir
