#include "LeirEngine/UI/UIWindow.h"
#include "LeirEngine/UI/UICanvas.h"
#include "LeirEngine/UI/Font.h"
#include <algorithm>
#include <cfloat>

namespace Leir {

UIWindow::UIWindow(const std::string& title)
    : m_Title(title)
{
    SetName("UIWindow");
    SetColor({0.13f, 0.13f, 0.15f, 1.0f});
    SetLayoutMode(LayoutMode::Free);
    SetSizePolicy(SizePolicy::Fixed);
    SetMinSize({320.0f, 200.0f});
    SetOverlayLayer(true); // windows float above the dock/viewports
    SetClip(true);
    SetActive(false);
}

UIWindow::~UIWindow()
{
    // Modal overlay is a canvas child, not ours; remove it before the window dies.
    if (m_ModalOverlay) {
        if (m_ModalOverlay->GetParent()) m_ModalOverlay->GetParent()->RemoveChild(m_ModalOverlay);
        delete m_ModalOverlay;
        m_ModalOverlay = nullptr;
    }
    if (m_Content) {
        RemoveChild(m_Content);
        delete m_Content;
        m_Content = nullptr;
    }
    if (m_CloseButton) { RemoveChild(m_CloseButton); delete m_CloseButton; m_CloseButton = nullptr; }
    if (m_MinButton)   { RemoveChild(m_MinButton);   delete m_MinButton;   m_MinButton = nullptr; }
    if (m_MaxButton)   { RemoveChild(m_MaxButton);   delete m_MaxButton;   m_MaxButton = nullptr; }
    if (m_TitleLabel)  { RemoveChild(m_TitleLabel);  delete m_TitleLabel;  m_TitleLabel = nullptr; }
    if (m_TitleBar)    { RemoveChild(m_TitleBar);    delete m_TitleBar;    m_TitleBar = nullptr; }
}

// ---- Lifecycle ----

void UIWindow::Show(UIWindow* parent)
{
    m_ParentWindow = parent;
    m_Modal = false;

    // If not yet on a canvas, add ourselves to the parent's canvas (or the root).
    if (!GetParent()) {
        UICanvas* canvas = nullptr;
        if (parent) {
            for (UIElement* e = parent; e; e = e->GetParent()) {
                if (auto* c = dynamic_cast<UICanvas*>(e)) { canvas = c; break; }
            }
        }
        if (canvas) {
            canvas->AddChild(this);
            // Modal overlay goes BELOW the window (insert before it).
            if (m_Modal) {
                // placeholder: overlay built in ShowModal
            }
        }
    }

    OnCreateChrome();
    SetActive(true);
    m_Visible = true;
    OnLayoutChrome();
    if (m_Content) m_Content->SetActive(true);
    BringToFront();
    OnShow();
}

void UIWindow::ShowModal(UIWindow* parent)
{
    m_ParentWindow = parent;
    m_Modal = true;

    UICanvas* canvas = nullptr;
    if (parent) {
        for (UIElement* e = parent; e; e = e->GetParent()) {
            if (auto* c = dynamic_cast<UICanvas*>(e)) { canvas = c; break; }
        }
    }

    if (!GetParent() && canvas) {
        // Build the modal overlay first (below the window), then add the window.
        m_ModalOverlay = new UIPanel();
        m_ModalOverlay->SetName("ModalOverlay");
        m_ModalOverlay->SetColor({0.0f, 0.0f, 0.0f, 0.55f});
        m_ModalOverlay->GetRect().anchor = AnchorSet::Stretch();
        m_ModalOverlay->SetHitTestable(true); // consume clicks → block parent
        canvas->AddChild(m_ModalOverlay);
        canvas->AddChild(this);
    }

    OnCreateChrome();
    SetActive(true);
    m_Visible = true;
    OnLayoutChrome();
    if (m_Content) m_Content->SetActive(true);
    BringToFront();
    OnShow();
}

void UIWindow::Close()
{
    if (!m_Visible)
        return;
    m_Visible = false;
    SetActive(false);
    OnClose();

    if (m_ModalOverlay) {
        if (m_ModalOverlay->GetParent()) m_ModalOverlay->GetParent()->RemoveChild(m_ModalOverlay);
        delete m_ModalOverlay;
        m_ModalOverlay = nullptr;
    }

    if (m_OnResult) m_OnResult(m_Result);
    if (m_OnClosed) m_OnClosed();
}

void UIWindow::Hide()
{
    if (!m_Visible)
        return;
    m_Visible = false;
    SetActive(false);
    if (m_ModalOverlay) {
        if (m_ModalOverlay->GetParent()) m_ModalOverlay->GetParent()->RemoveChild(m_ModalOverlay);
        delete m_ModalOverlay;
        m_ModalOverlay = nullptr;
    }
}

void UIWindow::BringToFront()
{
    // Internal mode: re-append at the end of the canvas children (draws on top).
    UICanvas* canvas = nullptr;
    for (UIElement* e = this; e; e = e->GetParent()) {
        if (auto* c = dynamic_cast<UICanvas*>(e)) { canvas = c; break; }
    }
    if (canvas) {
        canvas->RemoveChild(this);
        canvas->AddChild(this);
    }
}

// ---- State ----

void UIWindow::SetTitle(const std::string& title)
{
    m_Title = title;
    if (m_TitleLabel) m_TitleLabel->SetText(title);
}

// ---- Position & size ----

void UIWindow::SetPosition(const Vector2& pos)
{
    const Vector2 sz = GetSize();
    GetRect().anchor = AnchorSet::TopLeft();
    GetRect().offset = { pos.x, pos.y, pos.x + sz.x, pos.y + sz.y };
}

Vector2 UIWindow::GetPosition() const
{
    const auto& cr = GetComputedRect();
    return { cr.x, cr.y };
}

void UIWindow::SetSize(const Vector2& size)
{
    const Vector2 pos = GetPosition();
    Vector2 s = {
        std::clamp(size.x, m_MinSize.x, m_MaxSize.x),
        std::clamp(size.y, m_MinSize.y, m_MaxSize.y)
    };
    GetRect().anchor = AnchorSet::TopLeft();
    GetRect().offset = { pos.x, pos.y, pos.x + s.x, pos.y + s.y };
}

Vector2 UIWindow::GetSize() const
{
    const auto& cr = GetComputedRect();
    return { cr.z, cr.w };
}

void UIWindow::CenterOnParent()
{
    Vector2 host = { 1280.0f, 720.0f };
    for (UIElement* e = GetParent(); e; e = e->GetParent()) {
        if (auto* c = dynamic_cast<UICanvas*>(e)) {
            host = { c->GetScreenWidth(), c->GetScreenHeight() };
            break;
        }
    }
    const Vector2 sz = GetSize();
    SetPosition({ (host.x - sz.x) * 0.5f, (host.y - sz.y) * 0.5f });
}

// ---- Window state ----

void UIWindow::Minimize()
{
    if (m_Minimized) return;
    m_Minimized = true;
    SetActive(false);
}

void UIWindow::Maximize()
{
    if (m_Maximized) return;
    m_Maximized = true;
    UICanvas* canvas = nullptr;
    for (UIElement* e = this; e; e = e->GetParent()) {
        if (auto* c = dynamic_cast<UICanvas*>(e)) { canvas = c; break; }
    }
    if (canvas) {
        m_RestoredRect = GetComputedRect();
        GetRect().anchor = AnchorSet::Stretch();
        GetRect().offset = { 0.0f, 0.0f, 0.0f, 0.0f };
    }
}

void UIWindow::Restore()
{
    if (!m_Maximized && !m_Minimized) return;
    m_Maximized = false;
    m_Minimized = false;
    SetActive(true);
    if (m_Visible && !m_Maximized) {
        // Restore the saved normal rect.
        const Vector4 r = m_RestoredRect;
        GetRect().anchor = AnchorSet::TopLeft();
        GetRect().offset = { r.x, r.y, r.x + r.z, r.y + r.w };
    }
}

// ---- Content ----

void UIWindow::SetContent(UIElement* content)
{
    if (m_Content) {
        RemoveChild(m_Content);
        delete m_Content;
    }
    m_Content = content;
    if (m_Content) {
        AddChild(m_Content);
        m_Content->SetSizePolicy(SizePolicy::Fixed);
    }
    OnLayoutChrome();
}

// ---- Chrome (internal mode default) ----

void UIWindow::OnCreateChrome()
{
    if (m_TitleBar)
        return;

    if (m_HasTitleBar) {
        m_TitleBar = new UIPanel();
        m_TitleBar->SetName("WindowTitleBar");
        m_TitleBar->SetColor({0.09f, 0.09f, 0.11f, 1.0f});
        AddChild(m_TitleBar);

        m_TitleLabel = new UILabel();
        m_TitleLabel->SetName("WindowTitle");
        m_TitleLabel->SetText(m_Title);
        m_TitleLabel->SetColor({0.88f, 0.88f, 0.90f, 1.0f});
        m_TitleLabel->SetHitTestable(false);
        if (m_Font) m_TitleLabel->SetFont(m_Font);
        m_TitleBar->AddChild(m_TitleLabel);

        if (m_HasCloseButton) {
            m_CloseButton = new UIImage();
            m_CloseButton->SetName("WindowCloseBtn");
            m_CloseButton->SetColor({0.75f, 0.20f, 0.20f, 1.0f});
            m_TitleBar->AddChild(m_CloseButton);
        }
        if (m_HasMinimizeButton) {
            m_MinButton = new UIImage();
            m_MinButton->SetName("WindowMinBtn");
            m_MinButton->SetColor({0.45f, 0.45f, 0.5f, 1.0f});
            m_TitleBar->AddChild(m_MinButton);
        }
        if (m_HasMaximizeButton) {
            m_MaxButton = new UIImage();
            m_MaxButton->SetName("WindowMaxBtn");
            m_MaxButton->SetColor({0.45f, 0.45f, 0.5f, 1.0f});
            m_TitleBar->AddChild(m_MaxButton);
        }
    }
}

void UIWindow::OnDestroyChrome()
{
    // Chrome children are owned and deleted in the destructor;
    // this hook is for subclasses to clean up external resources.
}

void UIWindow::OnLayoutChrome()
{
    if (!m_TitleBar)
        return;

    const auto& cr = GetComputedRect();
    const float barW = cr.z;
    const float barH = kTitleBarHeight;

    m_TitleBar->GetRect().anchor = AnchorSet::TopLeft();
    m_TitleBar->GetRect().offset = { 0.0f, 0.0f, barW, barH };
    m_TitleBar->ComputeLayout({ barW, barH }, { cr.x, cr.y });

    if (m_TitleLabel) {
        m_TitleLabel->GetRect().anchor = AnchorSet::TopLeft();
        m_TitleLabel->GetRect().offset = { 8.0f, 0.0f, barW - 80.0f, barH };
        m_TitleLabel->ComputeLayout({ barW - 88.0f, barH }, { cr.x, cr.y });
    }

    float bx = barW - 6.0f;
    if (m_CloseButton) {
        bx -= kButtonSize;
        m_CloseButton->GetRect().anchor = AnchorSet::TopLeft();
        m_CloseButton->GetRect().offset = { bx, (barH - kButtonSize) * 0.5f, bx + kButtonSize, (barH - kButtonSize) * 0.5f + kButtonSize };
        m_CloseButton->ComputeLayout({ kButtonSize, kButtonSize }, { cr.x, cr.y });
    }
    if (m_MaxButton) {
        bx -= kButtonSize + 4.0f;
        m_MaxButton->GetRect().anchor = AnchorSet::TopLeft();
        m_MaxButton->GetRect().offset = { bx, (barH - kButtonSize) * 0.5f, bx + kButtonSize, (barH - kButtonSize) * 0.5f + kButtonSize };
        m_MaxButton->ComputeLayout({ kButtonSize, kButtonSize }, { cr.x, cr.y });
    }
    if (m_MinButton) {
        bx -= kButtonSize + 4.0f;
        m_MinButton->GetRect().anchor = AnchorSet::TopLeft();
        m_MinButton->GetRect().offset = { bx, (barH - kButtonSize) * 0.5f, bx + kButtonSize, (barH - kButtonSize) * 0.5f + kButtonSize };
        m_MinButton->ComputeLayout({ kButtonSize, kButtonSize }, { cr.x, cr.y });
    }

    // Content below the title bar.
    if (m_Content) {
        const float top = m_HasTitleBar ? barH : 0.0f;
        m_Content->GetRect().anchor = AnchorSet::TopLeft();
        m_Content->GetRect().offset = { 0.0f, top, cr.z, cr.w };
        m_Content->ComputeLayout({ cr.z, cr.w - top }, { cr.x, cr.y });
    }
}

// ---- Input (drag / resize / chrome buttons) ----

bool UIWindow::OnPointerDown(const Vector2& pos)
{
    const auto& cr = GetComputedRect();
    const float barH = m_HasTitleBar ? kTitleBarHeight : 0.0f;

    // Chrome buttons
    auto hit = [&](UIImage* b) {
        if (!b || !b->IsActive()) return false;
        const auto& br = b->GetComputedRect();
        return pos.x >= br.x && pos.x <= br.x + br.z && pos.y >= br.y && pos.y <= br.y + br.w;
    };
    if (hit(m_CloseButton)) { SetResult(WindowResult::Cancel); Close(); return true; }
    if (hit(m_MinButton)) { Minimize(); return true; }
    if (hit(m_MaxButton)) { if (m_Maximized) Restore(); else Maximize(); return true; }

    // Title bar drag
    if (m_HasTitleBar && pos.y >= cr.y && pos.y <= cr.y + barH) {
        m_Dragging = true;
        m_DragStart = GetPosition();
        m_DragCursorStart = pos;
        UICanvas* canvas = nullptr;
        for (UIElement* e = this; e; e = e->GetParent()) {
            if (auto* c = dynamic_cast<UICanvas*>(e)) { canvas = c; break; }
        }
        if (canvas) canvas->CapturePointer(this);
        return true;
    }

    // Resize edges
    if (m_Resizable && !m_Maximized) {
        const float b = kBorderSize;
        m_ResizeEdge = 0;
        if (pos.x <= cr.x + b) m_ResizeEdge |= 1;                 // left
        if (pos.x >= cr.x + cr.z - b) m_ResizeEdge |= 2;          // right
        if (pos.y <= cr.y + b) m_ResizeEdge |= 4;                 // top
        if (pos.y >= cr.y + cr.w - b) m_ResizeEdge |= 8;          // bottom
        if (m_ResizeEdge) {
            m_ResizeStartSize = GetSize();
            m_ResizeStartPos = GetPosition();
            m_DragCursorStart = pos;
            UICanvas* canvas = nullptr;
            for (UIElement* e = this; e; e = e->GetParent()) {
                if (auto* c = dynamic_cast<UICanvas*>(e)) { canvas = c; break; }
            }
            if (canvas) canvas->CapturePointer(this);
            return true;
        }
    }

    return false; // let the content handle it
}

void UIWindow::OnPointerMove(const Vector2& pos)
{
    if (m_Dragging) {
        Vector2 delta = pos - m_DragCursorStart;
        SetPosition(m_DragStart + delta);
        return;
    }
    if (m_ResizeEdge) {
        Vector2 delta = pos - m_DragCursorStart;
        Vector2 newSize = m_ResizeStartSize;
        Vector2 newPos = m_ResizeStartPos;
        if (m_ResizeEdge & 2) newSize.x += delta.x;              // right
        if (m_ResizeEdge & 8) newSize.y += delta.y;              // bottom
        if (m_ResizeEdge & 1) { newPos.x += delta.x; newSize.x -= delta.x; } // left
        if (m_ResizeEdge & 4) { newPos.y += delta.y; newSize.y -= delta.y; } // top
        newSize.x = std::clamp(newSize.x, m_MinSize.x, m_MaxSize.x);
        newSize.y = std::clamp(newSize.y, m_MinSize.y, m_MaxSize.y);
        SetPosition(newPos);
        SetSize(newSize);
        if (m_OnResized) m_OnResized((int)newSize.x, (int)newSize.y);
        OnLayoutChrome();
    }
}

bool UIWindow::OnPointerUp(const Vector2&)
{
    if (m_Dragging || m_ResizeEdge) {
        m_Dragging = false;
        m_ResizeEdge = 0;
        UICanvas* canvas = nullptr;
        for (UIElement* e = this; e; e = e->GetParent()) {
            if (auto* c = dynamic_cast<UICanvas*>(e)) { canvas = c; break; }
        }
        if (canvas) canvas->ReleasePointer();
        return true;
    }
    return false;
}

Vector2 UIWindow::GetMinSize() const
{
    return m_MinSize;
}

bool UIWindow::OwnsChild(const UIElement* child) const
{
    return child == m_Content || child == m_TitleBar || child == m_TitleLabel ||
           child == m_CloseButton || child == m_MinButton || child == m_MaxButton;
}

} // namespace Leir