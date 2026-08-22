#include "LeirEngine/Input/EventQueue.h"
#include "LeirEngine/Input/Keyboard.h"
#include "LeirEngine/Input/Mouse.h"
#include "LeirEngine/Input/Pointer.h"
#include "LeirEngine/Input/Touch.h"

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
                Keyboard::ProcessEvent(e);
                for (auto& h : m_KeyHooks) h(e);
            } else if constexpr (std::is_same_v<T, PointerEvent>) {
                Mouse::ProcessEvent(e);
                Touch::ProcessEvent(e);
                Pointer::ProcessEvent(e);
                for (auto& h : m_PointerHooks) h(e);
            } else if constexpr (std::is_same_v<T, CharEvent>) {
                for (auto& h : m_CharHooks) h(e);
            } else if constexpr (std::is_same_v<T, ScrollEvent>) {
                Mouse::ProcessScroll(e);
                for (auto& h : m_ScrollHooks) h(e);
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
