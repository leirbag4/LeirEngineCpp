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

// The global polling state (Keyboard/Mouse/Touch/Pointer) tracks the PRIMARY
// window only. Events from external windows (e.window != primary) update the
// polling state here would move the main editor's mouse/hover when the pointer
// is over an external window. nullptr primary = single-window apps (accept all).
static bool IsPrimaryWindow(const void* window)
{
    void* primary = InputManager::GetPrimaryWindow();
    if (!primary) return true;  // single-window app: no window filtering
    return !window || window == primary;
}

void EventQueue::Process()
{
    std::vector<InputEvent> events;
    {
        std::lock_guard lock(m_Mutex);
        events.swap(m_Queue);
    }

    for (const auto& event : events) {
        std::visit([this](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, KeyEvent>) {
                if (IsPrimaryWindow(e.window)) Keyboard::ProcessEvent(e);
                for (auto& [id, h] : m_KeyHooks) (void)id, h(e);
            } else if constexpr (std::is_same_v<T, PointerEvent>) {
                if (IsPrimaryWindow(e.window)) {
                    Mouse::ProcessEvent(e);
                    Touch::ProcessEvent(e);
                    Pointer::ProcessEvent(e);
                }
                for (auto& [id, h] : m_PointerHooks) (void)id, h(e);
            } else if constexpr (std::is_same_v<T, CharEvent>) {
                for (auto& [id, h] : m_CharHooks) (void)id, h(e);
            } else if constexpr (std::is_same_v<T, ScrollEvent>) {
                if (IsPrimaryWindow(e.window)) Mouse::ProcessScroll(e);
                for (auto& [id, h] : m_ScrollHooks) (void)id, h(e);
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
