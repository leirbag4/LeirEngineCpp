#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/Rect2D.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"
#include <string>
#include <vector>
#include <optional>

namespace Leir {

enum class LEIR_API LayoutMode {
    Free,
    Row,
    Column,
};

enum class LEIR_API SizePolicy {
    Fixed,
    Fill,
    Grow,
    Content,
};

class LEIR_API UIElement {
public:
    UIElement();
    virtual ~UIElement();

    Rect2D& GetRect() { return m_Rect; }
    const Rect2D& GetRect() const { return m_Rect; }

    void SetLayoutMode(LayoutMode mode) { m_LayoutMode = mode; }
    LayoutMode GetLayoutMode() const { return m_LayoutMode; }

    void SetSizePolicy(SizePolicy policy) { m_SizePolicy = policy; }
    SizePolicy GetSizePolicy() const { return m_SizePolicy; }

    void SetPadding(float left, float top, float right, float bottom);
    void SetSpacing(float spacing) { m_Spacing = spacing; }

    void SetPivot(const Vector2& pivot) { m_Rect.pivot = pivot; }
    const Vector2& GetPivot() const { return m_Rect.pivot; }

    void SetName(const std::string& name) { m_Name = name; }
    const std::string& GetName() const { return m_Name; }

    void SetParent(UIElement* parent);
    UIElement* GetParent() const { return m_Parent; }

    void AddChild(UIElement* child);
    // Inserts a child at a specific position in the children vector (used to
    // reorder tabs in a dock tab bar). Auto-reparents like AddChild; the index
    // is clamped to [0, size()].
    void InsertChildAt(UIElement* child, size_t index);
    void RemoveChild(UIElement* child);
    const std::vector<UIElement*>& GetChildren() const { return m_Children; }

    // Subtree-delete helpers (e.g. the editor's DeleteUiSubtree) use this to
    // decide whether they may delete `child` themselves or must leave it for
    // this element's destructor, which takes ownership of it. Composite
    // widgets (ScrollView, UITextArea, UIScrollbar) own some of their internal
    // children and delete them in their destructor; deleting such a child again
    // in a helper would be a double free. Default (plain containers): false.
    virtual bool OwnsChild(const UIElement* child) const { (void)child; return false; }

    const Vector4& GetComputedRect() const { return m_ComputedRect; }

    void SetColor(const Vector4& color) { m_Color = color; }
    const Vector4& GetColor() const { return m_Color; }

    void SetActive(bool active) { m_Active = active; }
    bool IsActive() const { return m_Active; }

    // Elements routed to the top overlay batch (drawn last, above viewports).
    void SetOverlayLayer(bool on) { m_IsOverlay = on; }
    bool IsOverlayLayer() const { return m_IsOverlay; }

    // When enabled, the element's computed rect becomes a clip region: descendants
    // are scissored to it (and culled entirely when fully outside).
    void SetClip(bool clip) { m_Clip = clip; }
    bool IsClipEnabled() const { return m_Clip; }

    virtual Vector2 GetMinSize() const;
    // Optional explicit minimum size override (layout still calls GetMinSize,
    // which returns this when set — useful for buttons that need extra width).
    void SetMinSize(const Vector2& minSize) { m_MinSizeOverride = minSize; }
    bool HasMinSizeOverride() const { return m_MinSizeOverride.has_value(); }

    virtual Vector2 GetContentSize() const;
    virtual void ComputeLayout(const Vector2& availableSize);

    UIElement* FindChildByName(const std::string& name);

    // Input events
    virtual void OnPointerEnter(const Vector2& pos) {}
    virtual void OnPointerExit() {}
    virtual void OnPointerMove(const Vector2& pos) {}
    virtual bool OnPointerDown(const Vector2& pos) { return false; }
    virtual bool OnPointerUp(const Vector2& pos) { return false; }
    virtual bool OnTextInput(uint32_t codepoint) { return false; }
    virtual bool OnKeyDown(int key) { return false; }
    virtual void OnFocus() {}
    virtual void OnBlur() {}

    // Mouse wheel. delta is in scroll lines (positive = scroll up/away).
    // Return true to consume the scroll (stop propagation to ancestors).
    virtual bool OnScroll(float delta) { return false; }

    // Events for pointer tracking (called by canvas)
    void SetHovered(bool h) { m_Hovered = h; }
    bool IsHovered() const { return m_Hovered; }

protected:
    // Derived GetMinSize() implementations consult this when a caller used
    // SetMinSize().
    const std::optional<Vector2>& GetMinSizeOverride() const { return m_MinSizeOverride; }
    virtual void OnLayoutComputed() {}

    Rect2D m_Rect;
    LayoutMode m_LayoutMode = LayoutMode::Free;
    SizePolicy m_SizePolicy = SizePolicy::Fixed;
    float m_Padding[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_Spacing = 0.0f;
    Vector4 m_ComputedRect = {0.0f, 0.0f, 0.0f, 0.0f};
    Vector4 m_Color = {1.0f, 1.0f, 1.0f, 1.0f};
    bool m_Active = true;
    bool m_Hovered = false;
    bool m_IsOverlay = false;
    bool m_Clip = false;
    std::string m_Name;

private:
    UIElement* m_Parent = nullptr;
    std::vector<UIElement*> m_Children;
    std::optional<Vector2> m_MinSizeOverride;
    Vector2 GetNaturalSize() const;
    void ComputeFreeLayout(const Vector2& availableSize);
    void ComputeRowLayout(const Vector2& availableSize);
    void ComputeColumnLayout(const Vector2& availableSize);
};

} // namespace Leir
