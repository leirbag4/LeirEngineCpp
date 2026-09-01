#include "LeirEngine/UI/UIWindow.h"
#include "LeirEngine/UI/UICanvas.h"
#include "LeirEngine/UI/Font.h"
#include "LeirEngine/Input/InputManager.h"
#include "LeirEngine/Core/Settings.h"
#include <GLFW/glfw3.h>
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
    m_ShadowEnabled = LeirSettings::Get().window.window_shadow;
}

UIWindow::~UIWindow()
{
    DestroyShadow();
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
        }
    }
    Activate();
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
    Activate();
}

void UIWindow::Activate()
{
    OnCreateChrome();
    SetActive(true);
    m_Visible = true;
    OnLayoutChrome();
    if (m_Content) m_Content->SetActive(true);
    CreateShadow();
    BringToFront();
    OnShow();
}

void UIWindow::OnLayoutComputed()
{
    UIPanel::OnLayoutComputed();
    OnLayoutChrome();
}

void UIWindow::Close()
{
    if (!m_Visible)
        return;
    m_Visible = false;
    SetActive(false);
    OnClose();
    DestroyShadow();

    // Detach from the canvas so the window is not a dangling child. The owner
    // (editor) may delete us any time after Close() — internal windows must not
    // remain as children of the canvas when closed.
    if (GetParent())
        GetParent()->RemoveChild(this);

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
    DestroyShadow();
    if (GetParent())
        GetParent()->RemoveChild(this);
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
        // Move the shadow layers first (stay behind the window), then the window on top.
        // Add in reverse order so the innermost (most opaque) layer is drawn last (top).
        for (auto it = m_ShadowLayers.rbegin(); it != m_ShadowLayers.rend(); ++it) {
            auto* layer = *it;
            if (layer->GetParent() == canvas) {
                canvas->RemoveChild(layer);
                canvas->AddChild(layer);
            }
        }
        canvas->RemoveChild(this);
        canvas->AddChild(this);
        UpdateShadowLayout();
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
    m_WindowPos = pos;
    m_Rect.anchor = AnchorSet::TopLeft();
    m_Rect.offset = { pos.x, pos.y, pos.x + m_WindowSize.x, pos.y + m_WindowSize.y };
}

Vector2 UIWindow::GetPosition() const
{
    return m_WindowPos;
}

void UIWindow::SetSize(const Vector2& size)
{
    m_WindowSize = {
        std::clamp(size.x, m_MinSize.x, m_MaxSize.x),
        std::clamp(size.y, m_MinSize.y, m_MaxSize.y)
    };
    m_Rect.anchor = AnchorSet::TopLeft();
    m_Rect.offset = { m_WindowPos.x, m_WindowPos.y, m_WindowPos.x + m_WindowSize.x, m_WindowPos.y + m_WindowSize.y };
}

Vector2 UIWindow::GetSize() const
{
    return m_WindowSize;
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

void UIWindow::SetWindowButtonIcons(std::shared_ptr<Texture2D> closeIcon,
                                    std::shared_ptr<Texture2D> minIcon,
                                    std::shared_ptr<Texture2D> maxIcon)
{
    m_CloseIcon = std::move(closeIcon);
    m_MinIcon = std::move(minIcon);
    m_MaxIcon = std::move(maxIcon);
    // If chrome is already built, apply immediately (else OnCreateChrome applies).
    if (m_CloseButton && m_CloseIcon) m_CloseButton->SetTexture(m_CloseIcon.get());
    if (m_MinButton && m_MinIcon) m_MinButton->SetTexture(m_MinIcon.get());
    if (m_MaxButton && m_MaxIcon) m_MaxButton->SetTexture(m_MaxIcon.get());
}

void UIWindow::OnCreateChrome()
{
    if (m_TitleBar)
        return;

    if (m_HasTitleBar) {
        m_TitleBar = new UIPanel();
        m_TitleBar->SetName("WindowTitleBar");
        m_TitleBar->SetColor({0.09f, 0.09f, 0.11f, 1.0f});
        // NOT hit-testable: the window itself handles events (drag, buttons,
        // resize). Otherwise the canvas hover lands on the title bar, and the
        // window's OnPointerMove/OnPointerDown (which handle button hover,
        // cursor changes, drag, resize) are never called.
        m_TitleBar->SetHitTestable(false);
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
            if (m_CloseIcon) m_CloseButton->SetTexture(m_CloseIcon.get());
            m_CloseButton->SetHitTestable(false);
            m_TitleBar->AddChild(m_CloseButton);
        }
        if (m_HasMinimizeButton) {
            m_MinButton = new UIImage();
            m_MinButton->SetName("WindowMinBtn");
            m_MinButton->SetColor({0.45f, 0.45f, 0.5f, 1.0f});
            if (m_MinIcon) m_MinButton->SetTexture(m_MinIcon.get());
            m_MinButton->SetHitTestable(false);
            m_TitleBar->AddChild(m_MinButton);
        }
        if (m_HasMaximizeButton) {
            m_MaxButton = new UIImage();
            m_MaxButton->SetName("WindowMaxBtn");
            m_MaxButton->SetColor({0.45f, 0.45f, 0.5f, 1.0f});
            if (m_MaxIcon) m_MaxButton->SetTexture(m_MaxIcon.get());
            m_MaxButton->SetHitTestable(false);
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

    // Content fills the visual rect edge-to-edge below the title bar (margin 0).
    // The resize border is a TRANSPARENT RING OUTSIDE the visual rect (see
    // GetHitRect): the content covers the full visual area, so the window
    // itself is never hovered inside — the ring beyond the edges catches the
    // resize/cursor events.
    if (m_Content) {
        const float top = m_HasTitleBar ? barH : 0.0f;
        m_Content->GetRect().anchor = AnchorSet::TopLeft();
        m_Content->GetRect().offset = { 0.0f, top, cr.z, cr.w - top };
        m_Content->ComputeLayout({ cr.z, cr.w - top }, { cr.x, cr.y });
    }

    UpdateShadowLayout();
}

// ---- Shadow helpers ----

void UIWindow::CreateShadow()
{
    if (!m_ShadowEnabled || !m_ShadowLayers.empty() || m_Maximized)
        return;

    UICanvas* canvas = nullptr;
    for (UIElement* e = this; e; e = e->GetParent()) {
        if (auto* c = dynamic_cast<UICanvas*>(e)) { canvas = c; break; }
    }
    if (!canvas) return;

    // Layered shadow: 3 quads with increasing size and decreasing alpha.
    // This simulates a soft drop shadow without a gradient texture.
    // L0 = innermost (darkest, smallest), L2 = outermost (faintest, largest).
    static const float kLayerAlpha[] = { 0.28f, 0.12f, 0.05f };
    static const float kLayerSize[]  = { 0.0f, 4.0f, 8.0f }; // extra beyond shadow size

    for (int i = 0; i < 3; ++i) {
        auto* layer = new UIImage();
        layer->SetName("WindowShadowLayer");
        layer->SetColor({0.0f, 0.0f, 0.0f, kLayerAlpha[i]});
        layer->SetHitTestable(false);
        layer->SetOverlayLayer(true); // same batch as the window, just below it
        m_ShadowLayers.push_back(layer);
    }

    // Insert into the canvas outermost-first so the canvas order is
    // [L2, L1, L0, W] — L0 (darkest) is drawn last among the layers, closest
    // to the window.
    for (auto it = m_ShadowLayers.rbegin(); it != m_ShadowLayers.rend(); ++it) {
        auto& children = canvas->GetChildren();
        int idx = 0;
        for (auto* c : children) {
            if (c == this) break;
            ++idx;
        }
        canvas->InsertChildAt(*it, idx);
    }

    UpdateShadowLayout();
}

void UIWindow::DestroyShadow()
{
    for (auto* layer : m_ShadowLayers) {
        if (layer->GetParent())
            layer->GetParent()->RemoveChild(layer);
        delete layer;
    }
    m_ShadowLayers.clear();
}

void UIWindow::UpdateShadowLayout()
{
    if (m_ShadowLayers.empty() || m_Maximized)
        return;

    const auto& cr = GetComputedRect();
    const float s = m_ShadowSize;

    for (size_t i = 0; i < m_ShadowLayers.size(); ++i) {
        const float extra = (i < 3) ? (i == 0 ? 0.0f : (i == 1 ? 4.0f : 8.0f)) : 0.0f;
        const float inset = s + extra;
        auto* layer = m_ShadowLayers[i];
        layer->GetRect().anchor = AnchorSet::TopLeft();
        // Shadow layers are children of the CANVAS. Their offset already holds
        // absolute coords (window position ± inset), so parentOffset = {0,0}
        // (the canvas origin). Passing window position as parentOffset would
        // double-add and make the shadow drift at 2× the window's movement.
        layer->GetRect().offset = {
            cr.x - inset, cr.y - inset,
            cr.x + cr.z + inset, cr.y + cr.w + inset
        };
        layer->ComputeLayout({ cr.z + 2.0f * inset, cr.w + 2.0f * inset },
                             { 0.0f, 0.0f });
    }
}

// ---- Resize border / hit-test / cursor ----

Vector4 UIWindow::GetHitRect() const
{
    const auto& cr = GetComputedRect();
    if (!m_Resizable || m_Maximized)
        return cr;
    const float b = m_ResizeBorderSize;
    return {cr.x - b, cr.y - b, cr.z + 2.0f * b, cr.w + 2.0f * b};
}

int UIWindow::HitTestZone(const Vector2& pos, bool& onTitleBar) const
{
    const auto& cr = GetComputedRect();
    const auto& hr = GetHitRect();
    const float barH = m_HasTitleBar ? kTitleBarHeight : 0.0f;
    onTitleBar = false;

    if (!m_Resizable || m_Maximized) {
        if (m_HasTitleBar && pos.y >= cr.y && pos.y <= cr.y + barH)
            onTitleBar = true;
        return 0; // HTCLIENT
    }

    // The 3×3 grid operates on the HIT rect (which includes the transparent
    // resize ring outside the visual rect), so the edge zones cover the ring.
    const float b = m_ResizeBorderSize;
    int row = 1, col = 1;
    if (pos.y >= hr.y && pos.y < hr.y + b) row = 0;
    else if (pos.y >= hr.y + hr.w - b && pos.y < hr.y + hr.w) row = 2;
    if (pos.x >= hr.x && pos.x < hr.x + b) col = 0;
    else if (pos.x >= hr.x + hr.z - b && pos.x < hr.x + hr.z) col = 2;

    // HT* codes mapped by (row,col):
    //   HTTOPLEFT=13  HTTOP=12  HTTOPRIGHT=14
    //   HTLEFT=10     HTCLIENT=0 HTRIGHT=11
    //   HTBOTTOMLEFT=16 HTBOTTOM=15 HTBOTTOMRIGHT=17
    static const int kHT[3][3] = {
        { 13, 12, 14 },
        { 10,  0, 11 },
        { 16, 15, 17 },
    };
    int zone = kHT[row][col];

    // If it's HTCLIENT but on the title bar → mark as caption (draggable).
    // The title bar check uses the VISUAL rect (the bar is inside the window,
    // not in the outer ring).
    if (zone == 0 && m_HasTitleBar && pos.y >= cr.y && pos.y <= cr.y + barH)
        onTitleBar = true;

    return zone;
}

// ---- Input (drag / resize / chrome buttons / cursor) ----

bool UIWindow::OnPointerDown(const Vector2& pos)
{
    // Chrome buttons (hit-test first, regardless of zone).
    auto hit = [&](UIImage* b) {
        if (!b || !b->IsActive()) return false;
        const auto& br = b->GetComputedRect();
        return pos.x >= br.x && pos.x <= br.x + br.z && pos.y >= br.y && pos.y <= br.y + br.w;
    };
    if (hit(m_CloseButton)) { SetResult(WindowResult::Cancel); Close(); return true; }
    if (hit(m_MinButton)) { Minimize(); return true; }
    if (hit(m_MaxButton)) { if (m_Maximized) Restore(); else Maximize(); return true; }

    // Determine the hit zone (resize edges, corners, title bar).
    bool onTitleBar = false;
    int zone = HitTestZone(pos, onTitleBar);

    // Resize corners/edges (HTLEFT=10, HTRIGHT=11, HTTOP=12, HTTOPLEFT=13,
    // HTTOPRIGHT=14, HTBOTTOM=15, HTBOTTOMLEFT=16, HTBOTTOMRIGHT=17).
    if (zone >= 10 && zone <= 17) {
        m_ResizeEdge = 0;
        if (zone == 10 || zone == 13 || zone == 16) m_ResizeEdge |= 1;  // left
        if (zone == 11 || zone == 14 || zone == 17) m_ResizeEdge |= 2;  // right
        if (zone == 12 || zone == 13 || zone == 14) m_ResizeEdge |= 4;  // top
        if (zone == 15 || zone == 16 || zone == 17) m_ResizeEdge |= 8;  // bottom

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

    // Title bar: drag or double-click to maximize/restore.
    if (onTitleBar) {
        const double now = glfwGetTime();
        const bool isDouble = (now - m_LastClickTime) < 0.5 &&
                              (pos - m_LastClickPos).Length() < 8.0f;
        m_LastClickTime = now;
        m_LastClickPos = pos;

        if (isDouble) {
            if (m_Maximized) Restore(); else Maximize();
            return true;
        }

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

    return false; // let the content handle it
}

void UIWindow::OnPointerMove(const Vector2& pos)
{
    // During drag or resize, handle movement.
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

        // Clamp size and compensate position so the opposite edge stays fixed.
        float clampedX = std::clamp(newSize.x, m_MinSize.x, m_MaxSize.x);
        float clampedY = std::clamp(newSize.y, m_MinSize.y, m_MaxSize.y);
        if (m_ResizeEdge & 1) newPos.x += newSize.x - clampedX;
        if (m_ResizeEdge & 4) newPos.y += newSize.y - clampedY;
        newSize.x = clampedX;
        newSize.y = clampedY;

        SetPosition(newPos);
        SetSize(newSize);
        if (m_OnResized) m_OnResized((int)newSize.x, (int)newSize.y);
        OnLayoutChrome();
        return;
    }

    // Not dragging: hover feedback — chrome button highlight + resize cursor.
    const auto& cr = GetComputedRect();

    auto hit = [&](UIImage* b) {
        if (!b || !b->IsActive()) return false;
        const auto& br = b->GetComputedRect();
        return pos.x >= br.x && pos.x <= br.x + br.z && pos.y >= br.y && pos.y <= br.y + br.w;
    };
    const bool overClose = hit(m_CloseButton);
    const bool overMin = hit(m_MinButton);
    const bool overMax = hit(m_MaxButton);

    if (m_CloseButton) m_CloseButton->SetColor(overClose ? Vector4(0.55f, 0.20f, 0.20f, 1.0f) : Vector4(0.75f, 0.20f, 0.20f, 1.0f));
    if (m_MinButton) m_MinButton->SetColor(overMin ? Vector4(0.55f, 0.55f, 0.62f, 1.0f) : Vector4(0.45f, 0.45f, 0.5f, 1.0f));
    if (m_MaxButton) m_MaxButton->SetColor(overMax ? Vector4(0.55f, 0.55f, 0.62f, 1.0f) : Vector4(0.45f, 0.45f, 0.5f, 1.0f));

    // Windows-style resize cursor based on the hit-test zone.
    bool onTitleBar = false;
    int zone = HitTestZone(pos, onTitleBar);
    switch (zone) {
        case 10: case 11: InputManager::SetCursorStyle(CursorStyle::ResizeEW); break;
        case 12: case 15: InputManager::SetCursorStyle(CursorStyle::ResizeNS); break;
        case 13: case 17: InputManager::SetCursorStyle(CursorStyle::ResizeNWSE); break;
        case 14: case 16: InputManager::SetCursorStyle(CursorStyle::ResizeNESW); break;
        default: InputManager::SetCursorStyle(CursorStyle::Arrow); break;
    }
}

bool UIWindow::OnPointerUp(const Vector2&)
{
    if (m_Dragging || m_ResizeEdge) {
        m_Dragging = false;
        m_ResizeEdge = 0;
        InputManager::SetCursorStyle(CursorStyle::Arrow);
        UICanvas* canvas = nullptr;
        for (UIElement* e = this; e; e = e->GetParent()) {
            if (auto* c = dynamic_cast<UICanvas*>(e)) { canvas = c; break; }
        }
        if (canvas) canvas->ReleasePointer();
        return true;
    }
    return false;
}

void UIWindow::OnPointerExit()
{
    if (m_CloseButton) m_CloseButton->SetColor(Vector4(0.75f, 0.20f, 0.20f, 1.0f));
    if (m_MinButton) m_MinButton->SetColor(Vector4(0.45f, 0.45f, 0.5f, 1.0f));
    if (m_MaxButton) m_MaxButton->SetColor(Vector4(0.45f, 0.45f, 0.5f, 1.0f));
    if (!m_Dragging && !m_ResizeEdge)
        InputManager::SetCursorStyle(CursorStyle::Arrow);
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