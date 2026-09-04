#include "LeirEngine/UI/UICanvas.h"
#include "LeirEngine/Input/EventQueue.h"
#include "LeirEngine/Core/Settings.h"
#include "LeirEngine/Core/Log.h"
#include <algorithm>
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

    // Filter events by the bound window (if set). Multi-window: each canvas
    // receives only events for its own window. Single-window (m_InputWindow==nullptr)
    // accepts all events so the default behaviour is unchanged.
    auto filtered = [this](const void* w) {
        return m_InputWindow && w && w != m_InputWindow;
    };

    m_HookTokens[0] = eq.AddPointerHook([this, filtered](const PointerEvent& e) {
        if (filtered(e.window)) return;
        ProcessPointerEvent(e);
    });

    m_HookTokens[1] = eq.AddCharHook([this, filtered](const CharEvent& e) {
        if (filtered(e.window)) return;
        SendTextInput(e.codepoint);
    });

    m_HookTokens[2] = eq.AddKeyHook([this, filtered](const KeyEvent& e) {
        if (filtered(e.window)) return;
        if (e.action == EventAction::Press || e.action == EventAction::Repeat)
            SendKeyDown(static_cast<int>(e.key));
    });

    m_HookTokens[3] = eq.AddScrollHook([this, filtered](const ScrollEvent& e) {
        if (filtered(e.window)) return;
        ProcessScrollEvent(e);
    });
}

void UICanvas::DisconnectFromInputSystem()
{
    auto& eq = EventQueue::Get();
    if (m_HookTokens[0]) eq.RemovePointerHook(m_HookTokens[0]);
    if (m_HookTokens[1]) eq.RemoveCharHook(m_HookTokens[1]);
    if (m_HookTokens[2]) eq.RemoveKeyHook(m_HookTokens[2]);
    if (m_HookTokens[3]) eq.RemoveScrollHook(m_HookTokens[3]);
    for (auto& t : m_HookTokens) t = 0;
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
    HitTestRecursive(this, screenPos, outElement, nullptr);
    return outElement != nullptr;
}

void UICanvas::HitTestRecursive(UIElement* element, const Vector2& pos, UIElement*& out,
                                 const Vector4* clip)
{
    if (!element->IsActive() || !element->IsHitTestable() || out)
        return;

    const auto& r = element->GetHitRect();

    // Effective clip: intersect this element's rect with the active clip when
    // clipping is enabled, mirroring UIRenderer::RenderElement so hit-testing
    // never reaches content that is visually scissored away (e.g. console lines
    // scrolled over the header, or content under a scrollbar strip).
    Vector4 localClip;
    const Vector4* effClip = clip;
    if (element->IsClipEnabled()) {
        if (clip) {
            localClip.x = std::max(clip->x, r.x);
            localClip.y = std::max(clip->y, r.y);
            localClip.z = std::min(clip->x + clip->z, r.x + r.z) - localClip.x;
            localClip.w = std::min(clip->y + clip->w, r.y + r.w) - localClip.y;
        } else {
            localClip = r;
        }
        if (localClip.z <= 0.0f || localClip.w <= 0.0f)
            return; // fully clipped away -> subtree is invisible, skip
        effClip = &localClip;
    } else if (effClip) {
        // Fast reject: fully outside the active clip.
        if (r.x + r.z <= effClip->x || r.x >= effClip->x + effClip->z ||
            r.y + r.w <= effClip->y || r.y >= effClip->y + effClip->w)
            return;
    }

    // The pointer must lie inside the effective clip for any of this subtree
    // to be a valid target.
    if (effClip) {
        if (pos.x < effClip->x || pos.x > effClip->x + effClip->z ||
            pos.y < effClip->y || pos.y > effClip->y + effClip->w)
            return;
    }

    const bool inside = pos.x >= r.x && pos.x <= r.x + r.z &&
                        pos.y >= r.y && pos.y <= r.y + r.w;

    const auto& children = element->GetChildren();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        HitTestRecursive(*it, pos, out, effClip);
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
            XConsole::Trace("[UIEvent] source={} action={} pos=({:.1f},{:.1f}) btn={} -> captured '{}'",
                (int)e.source, (int)e.action, pos.x, pos.y, (int)e.button,
                m_CaptureElement->GetName().c_str());
        if (e.action == EventAction::Move)
            m_CaptureElement->OnPointerMove(pos);
        else if (e.action == EventAction::Release) {
            m_CaptureElement->OnPointerUp(pos);
            if (trace)
                XConsole::Trace("[UIEvent] Release -> captured '{}' ended",
                    m_CaptureElement->GetName().c_str());
            XConsole::Trace("[Canvas] ReleaseCapture");
            m_CaptureElement = nullptr;
            // A captured release also ends the pointer-down state (otherwise
            // m_PointerDown stays true and hover would stay frozen forever).
            m_PointerDown = false;
        }
        return;
    }

    UIElement* hit = nullptr;
    HitTest(pos, hit);
    XConsole::Trace("[Canvas] HitTest: {} (prev hover: {})",
        hit ? hit->GetName().c_str() : "null",
        m_HoveredElement ? m_HoveredElement->GetName().c_str() : "null");

    // Hover is a no-button state. While a pointer button is held (drag in
    // progress) and no element captured the pointer, the cursor is dragging, not
    // hovering — do NOT change hover state (SetHovered / OnPointerEnter/Exit).
    // Otherwise dragging from elsewhere over e.g. a tree row highlights it.
    // Move forwarding below is kept so non-capturing drags (UISlider) still work.
    const bool dragging = m_PointerDown && !m_CaptureElement;
    if (!dragging && hit != m_HoveredElement) {
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
            XConsole::Trace("[UIEvent] source={} action={} pos=({:.1f},{:.1f}) hover -> '{}'",
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
                XConsole::Trace("[UIEvent] Press pos=({:.1f},{:.1f}) hit='{}' tried=[{}] handled='{}' focus='{}' capture='{}'",
                    pos.x, pos.y, hit->GetName().c_str(), tried.c_str(),
                    target ? target->GetName().c_str() : "null",
                    m_FocusElement ? m_FocusElement->GetName().c_str() : "null",
                    m_CaptureElement ? m_CaptureElement->GetName().c_str() : "null");
        } else {
            XConsole::Trace("[Canvas] Press on empty area, clearing focus");
            if (trace)
                XConsole::Trace("[UIEvent] Press pos=({:.1f},{:.1f}) hit=null (empty area), focus cleared",
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
                XConsole::Trace("[UIEvent] Release pos=({:.1f},{:.1f}) hit='{}' handled='{}'",
                    pos.x, pos.y, hit->GetName().c_str(),
                    target ? target->GetName().c_str() : "null");
        } else if (trace) {
            XConsole::Trace("[UIEvent] Release pos=({:.1f},{:.1f}) hit=null", pos.x, pos.y);
        }
    }
}

void UICanvas::NotifyPointerLeave()
{
    if (m_HoveredElement) {
        m_HoveredElement->OnPointerExit();
        m_HoveredElement->SetHovered(false);
        m_HoveredElement = nullptr;
    }
    m_FocusElement = nullptr;
    m_PointerDown = false;
}

void UICanvas::ProcessScrollEvent(const ScrollEvent& e)
{
    XConsole::Trace("[Canvas] Scroll: ({:.1f},{:.1f}) hover={}",
        e.offset.x, e.offset.y,
        m_HoveredElement ? m_HoveredElement->GetName().c_str() : "null");

    if (!m_HoveredElement) return;
    // Propagate up the parent chain until an element consumes the scroll.
    UIElement* target = m_HoveredElement;
    while (target && !target->OnScroll(e.offset.y))
        target = target->GetParent();
}

void UICanvas::SetFocus(UIElement* element)
{
    if (m_FocusElement == element) return;
    XConsole::Trace("[Canvas] Focus change: {} -> {}",
        m_FocusElement ? m_FocusElement->GetName().c_str() : "null",
        element ? element->GetName().c_str() : "null");
    if (LeirSettings::Get().debug.ui_event_log)
        XConsole::Trace("[UIEvent] Focus change: '{}' -> '{}'",
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
        XConsole::Trace("[UIEvent] Text '{}' -> focus '{}'",
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
        XConsole::Trace("[UIEvent] Key {} -> focus '{}'",
            key, m_FocusElement ? m_FocusElement->GetName().c_str() : "null");
    if (m_FocusElement)
        m_FocusElement->OnKeyDown(key);
}

} // namespace Leir
