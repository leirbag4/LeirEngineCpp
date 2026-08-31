#pragma once
#include "LeirEngine/Input/InputEvent.h"
#include "LeirEngine/Core/Export.h"
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Leir {

// Thread-safe event queue. Events are pushed from GLFW callbacks (InputManager)
// and processed once per frame in CoreApplication::Run. Multiple observers can
// register hooks; multi-window apps filter by the event's `window` field.
class LEIR_API EventQueue {
public:
    static EventQueue& Get();

    void Push(const InputEvent& event);
    void Process();

    using KeyHook = std::function<void(const KeyEvent&)>;
    using CharHook = std::function<void(const CharEvent&)>;
    using PointerHook = std::function<void(const PointerEvent&)>;
    using ScrollHook = std::function<void(const ScrollEvent&)>;
    using HookId = uint64_t;

    // Register a hook; returns a token to remove it later. Multiple hooks of
    // the same type coexist (the main canvas + the gizmo log recorder + the
    // external window canvases). Use Remove*Hook(id) to unsubscribe.
    HookId AddKeyHook(KeyHook hook) { return AddHook(m_KeyHooks, std::move(hook)); }
    HookId AddCharHook(CharHook hook) { return AddHook(m_CharHooks, std::move(hook)); }
    HookId AddPointerHook(PointerHook hook) { return AddHook(m_PointerHooks, std::move(hook)); }
    HookId AddScrollHook(ScrollHook hook) { return AddHook(m_ScrollHooks, std::move(hook)); }

    void RemoveKeyHook(HookId id) { RemoveHook(m_KeyHooks, id); }
    void RemoveCharHook(HookId id) { RemoveHook(m_CharHooks, id); }
    void RemovePointerHook(HookId id) { RemoveHook(m_PointerHooks, id); }
    void RemoveScrollHook(HookId id) { RemoveHook(m_ScrollHooks, id); }

    // Legacy: replace the listener list (keeps single-hook call sites working).
    void SetKeyHook(KeyHook hook) { m_KeyHooks = { { NextId(), std::move(hook) } }; }
    void SetCharHook(CharHook hook) { m_CharHooks = { { NextId(), std::move(hook) } }; }
    void SetPointerHook(PointerHook hook) { m_PointerHooks = { { NextId(), std::move(hook) } }; }
    void SetScrollHook(ScrollHook hook) { m_ScrollHooks = { { NextId(), std::move(hook) } }; }

    void ClearHooks();

private:
    EventQueue() = default;

    template <typename Hook>
    HookId AddHook(std::vector<std::pair<HookId, Hook>>& list, Hook hook) {
        HookId id = NextId();
        list.emplace_back(id, std::move(hook));
        return id;
    }

    template <typename Hook>
    void RemoveHook(std::vector<std::pair<HookId, Hook>>& list, HookId id) {
        for (size_t i = 0; i < list.size(); ++i) {
            if (list[i].first == id) {
                list.erase(list.begin() + (long)i);
                return;
            }
        }
    }

    static HookId NextId() {
        static HookId s_Next = 1;
        return s_Next++;
    }

    std::mutex m_Mutex;
    std::vector<InputEvent> m_Queue;

    std::vector<std::pair<HookId, KeyHook>> m_KeyHooks;
    std::vector<std::pair<HookId, CharHook>> m_CharHooks;
    std::vector<std::pair<HookId, PointerHook>> m_PointerHooks;
    std::vector<std::pair<HookId, ScrollHook>> m_ScrollHooks;
};

} // namespace Leir