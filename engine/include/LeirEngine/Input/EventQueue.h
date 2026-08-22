#pragma once
#include "LeirEngine/Input/InputEvent.h"
#include "LeirEngine/Core/Export.h"
#include <functional>
#include <mutex>
#include <vector>

namespace Leir {

class LEIR_API EventQueue {
public:
    static EventQueue& Get();

    void Push(const InputEvent& event);
    void Process();

    using KeyHook = std::function<void(const KeyEvent&)>;
    using CharHook = std::function<void(const CharEvent&)>;
    using PointerHook = std::function<void(const PointerEvent&)>;
    using ScrollHook = std::function<void(const ScrollEvent&)>;

    // Replace the listener list (keeps legacy single-hook call sites working).
    void SetKeyHook(KeyHook hook) { m_KeyHooks = { std::move(hook) }; }
    void SetCharHook(CharHook hook) { m_CharHooks = { std::move(hook) }; }
    void SetPointerHook(PointerHook hook) { m_PointerHooks = { std::move(hook) }; }
    void SetScrollHook(ScrollHook hook) { m_ScrollHooks = { std::move(hook) }; }

    // Add an extra listener (multiple observers allowed, e.g. the editor's
    // gizmo log recorder alongside the UICanvas).
    void AddKeyHook(KeyHook hook) { m_KeyHooks.push_back(std::move(hook)); }
    void AddCharHook(CharHook hook) { m_CharHooks.push_back(std::move(hook)); }
    void AddPointerHook(PointerHook hook) { m_PointerHooks.push_back(std::move(hook)); }
    void AddScrollHook(ScrollHook hook) { m_ScrollHooks.push_back(std::move(hook)); }

    void ClearHooks();

private:
    EventQueue() = default;

    std::mutex m_Mutex;
    std::vector<InputEvent> m_Queue;

    std::vector<KeyHook> m_KeyHooks;
    std::vector<CharHook> m_CharHooks;
    std::vector<PointerHook> m_PointerHooks;
    std::vector<ScrollHook> m_ScrollHooks;
};

} // namespace Leir
