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

    void SetKeyHook(KeyHook hook) { m_KeyHook = hook; }
    void SetCharHook(CharHook hook) { m_CharHook = hook; }
    void SetPointerHook(PointerHook hook) { m_PointerHook = hook; }
    void SetScrollHook(ScrollHook hook) { m_ScrollHook = hook; }

    void ClearHooks();

private:
    EventQueue() = default;

    std::mutex m_Mutex;
    std::vector<InputEvent> m_Queue;

    KeyHook m_KeyHook;
    CharHook m_CharHook;
    PointerHook m_PointerHook;
    ScrollHook m_ScrollHook;
};

} // namespace Leir
