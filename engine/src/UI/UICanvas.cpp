#include "LeirEngine/UI/UICanvas.h"
#include "LeirEngine/Input/EventQueue.h"
#include "LeirEngine/Core/Settings.h"
#include "LeirEngine/Core/Log.h"
#include <string>

namespace Leir {

UICanvas::UICanvas()
{
    SetName("Canvas");
    m_Rect.anchor = AnchorSet::Stretch();
    m_Rect.offset = {};
}

UICanvas::~UICanvas()
{
    DisconnectFromInputSystem();
}

void UICanvas::ConnectToInputSystem()
{
    auto& eq = EventQueue::Get();

    eq.SetPointerHook([this](const PointerEvent& e) {
        ProcessPointerEvent(e);
    });

    eq.SetCharHook([this](const CharEvent& e) {
        SendTextInput(e.codepoint);
    });

    eq.SetKeyHook([this](const KeyEvent& e) {
        if (e.action == EventAction::Press || e.action == EventAction::Repeat)
            SendKeyDown(static_cast<int>(e.key));
    });
}

void UICanvas::DisconnectFromInputSystem()
{
    EventQueue::Get().ClearHooks();
}

void UICanvas::SetScreenSize(float width, float height)
{
    m_ScreenWidth = width;
    m_ScreenHeight = height;
}

void UICanvas::UpdateLayout()
{
    ComputeLayout({m_ScreenWidth, m_ScreenHeight});
}

bool UICanvas::HitTest(const Vector2& screenPos, UIElement*& outElement)
{
    outElement = nullptr;
    HitTestRecursive(this, screenPos, outElement);
    return outElement != nullptr;
}

void UICanvas::HitTestRecursive(UIElement* element, const Vector2& pos, UIElement*& out)
{
    if (!element->IsActive())
        return;

    const auto& r = element->GetComputedRect();
    bool inside = pos.x >= r.x && pos.x <= r.x + r.z &&
                  pos.y >= r.y && pos.y <= r.y + r.w;

    const auto& children = element->GetChildren();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        HitTestRecursive(*it, pos, out);
        if (out) return;
    }

    if (inside)
        out = element;
}

void UICanvas::ProcessPointerEvent(const PointerEvent& e)
{
    Vector2 pos = e.position;
    const bool trace = LeirSettings::Get().debug.ui_event_log;

    XConsole::Trace("[Canvas] ProcessPointerEvent: source={} action={} pos=({:.1f},{:.1f}) btn={} capture={}",
        (int)e.source, (int)e.action, pos.x, pos.y, (int)e.button,
        m_CaptureElement ? m_CaptureElement->GetName().c_str() : "null");

    // If an element has captured the pointer, route everything to it
    if (m_CaptureElement && e.action != EventAction::Press) {
        XConsole::Trace("[Canvas] Captured -> {} (action={})",
            m_CaptureElement->GetName().c_str(), (int)e.action);
        if (trace)
            XConsole::Println("[UIEvent] source={} action={} pos=({:.1f},{:.1f}) btn={} -> captured '{}'",
                (int)e.source, (int)e.action, pos.x, pos.y, (int)e.button,
                m_CaptureElement->GetName().c_str());
        if (e.action == EventAction::Move)
            m_CaptureElement->OnPointerMove(pos);
        else if (e.action == EventAction::Release) {
            m_CaptureElement->OnPointerUp(pos);
            if (trace)
                XConsole::Println("[UIEvent] Release -> captured '{}' ended",
                    m_CaptureElement->GetName().c_str());
            XConsole::Trace("[Canvas] ReleaseCapture");
            m_CaptureElement = nullptr;
        }
        return;
    }

    UIElement* hit = nullptr;
    HitTest(pos, hit);
    XConsole::Trace("[Canvas] HitTest: {} (prev hover: {})",
        hit ? hit->GetName().c_str() : "null",
        m_HoveredElement ? m_HoveredElement->GetName().c_str() : "null");

    // Hover tracking (only when no capture)
    if (hit != m_HoveredElement) {
        if (m_HoveredElement) {
            m_HoveredElement->OnPointerExit();
            m_HoveredElement->SetHovered(false);
        }
        m_HoveredElement = hit;
        if (m_HoveredElement) {
            m_HoveredElement->SetHovered(true);
            m_HoveredElement->OnPointerEnter(pos);
        }
        if (trace)
            XConsole::Println("[UIEvent] source={} action={} pos=({:.1f},{:.1f}) hover -> '{}'",
                (int)e.source, (int)e.action, pos.x, pos.y,
                m_HoveredElement ? m_HoveredElement->GetName().c_str() : "null");
    }

    if (m_HoveredElement && !m_CaptureElement) {
        m_HoveredElement->OnPointerMove(pos);
    }

    if (e.action == EventAction::Press) {
        m_PointerDown = true;
        if (hit) {
            // Propagate OnPointerDown up the parent chain if child returns false
            UIElement* target = hit;
            std::string tried;
            if (trace) tried = hit->GetName();
            while (target && !target->OnPointerDown(pos)) {
                target = target->GetParent();
                if (trace && target) tried += " -> " + target->GetName();
            }

            XConsole::Trace("[Canvas] Press target: {} (hit: {})",
                target ? target->GetName().c_str() : "null",
                hit->GetName().c_str());

            if (target)
                SetFocus(target);
            else
                SetFocus(hit);

            if (trace)
                XConsole::Println("[UIEvent] Press pos=({:.1f},{:.1f}) hit='{}' tried=[{}] handled='{}' focus='{}' capture='{}'",
                    pos.x, pos.y, hit->GetName().c_str(), tried.c_str(),
                    target ? target->GetName().c_str() : "null",
                    m_FocusElement ? m_FocusElement->GetName().c_str() : "null",
                    m_CaptureElement ? m_CaptureElement->GetName().c_str() : "null");
        } else {
            XConsole::Trace("[Canvas] Press on empty area, clearing focus");
            if (trace)
                XConsole::Println("[UIEvent] Press pos=({:.1f},{:.1f}) hit=null (empty area), focus cleared",
                    pos.x, pos.y);
            ClearFocus();
        }
    }

    if (e.action == EventAction::Release && !m_CaptureElement) {
        m_PointerDown = false;
        if (hit) {
            UIElement* target = hit;
            while (target && !target->OnPointerUp(pos))
                target = target->GetParent();
            XConsole::Trace("[Canvas] Release target: {}",
                target ? target->GetName().c_str() : "null");
            if (trace)
                XConsole::Println("[UIEvent] Release pos=({:.1f},{:.1f}) hit='{}' handled='{}'",
                    pos.x, pos.y, hit->GetName().c_str(),
                    target ? target->GetName().c_str() : "null");
        } else if (trace) {
            XConsole::Println("[UIEvent] Release pos=({:.1f},{:.1f}) hit=null", pos.x, pos.y);
        }
    }
}

void UICanvas::SetFocus(UIElement* element)
{
    if (m_FocusElement == element) return;
    XConsole::Trace("[Canvas] Focus change: {} -> {}",
        m_FocusElement ? m_FocusElement->GetName().c_str() : "null",
        element ? element->GetName().c_str() : "null");
    if (LeirSettings::Get().debug.ui_event_log)
        XConsole::Println("[UIEvent] Focus change: '{}' -> '{}'",
            m_FocusElement ? m_FocusElement->GetName().c_str() : "null",
            element ? element->GetName().c_str() : "null");
    if (m_FocusElement)
        m_FocusElement->OnBlur();
    m_FocusElement = element;
    if (m_FocusElement)
        m_FocusElement->OnFocus();
}

void UICanvas::SendTextInput(uint32_t codepoint)
{
    XConsole::Trace("[Canvas] SendTextInput: codepoint={} ('{}') focus={}",
        codepoint, (char)codepoint,
        m_FocusElement ? m_FocusElement->GetName().c_str() : "null");
    if (LeirSettings::Get().debug.ui_event_log)
        XConsole::Println("[UIEvent] Text '{}' -> focus '{}'",
            (char)codepoint,
            m_FocusElement ? m_FocusElement->GetName().c_str() : "null");
    if (m_FocusElement)
        m_FocusElement->OnTextInput(codepoint);
}

void UICanvas::SendKeyDown(int key)
{
    XConsole::Trace("[Canvas] SendKeyDown: key={} focus={}",
        key, m_FocusElement ? m_FocusElement->GetName().c_str() : "null");
    if (LeirSettings::Get().debug.ui_event_log)
        XConsole::Println("[UIEvent] Key {} -> focus '{}'",
            key, m_FocusElement ? m_FocusElement->GetName().c_str() : "null");
    if (m_FocusElement)
        m_FocusElement->OnKeyDown(key);
}

} // namespace Leir
