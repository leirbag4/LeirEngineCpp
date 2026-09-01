#pragma once

/**
 * @file UIElement.h
 * @brief Base class for all UI elements: layout, hierarchy, input, clipping and hit-testing.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/Rect2D.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"
#include <string>
#include <vector>
#include <optional>

namespace Leir {

/**
 * @brief Layout mode for an element's children.
 * @ingroup UI
 */
enum class LEIR_API LayoutMode {
    Free,   ///< Children positioned by anchors (Free layout).
    Row,    ///< Children laid out horizontally with spacing.
    Column, ///< Children laid out vertically with spacing.
};

/**
 * @brief Size policy for an element within its parent.
 * @ingroup UI
 */
enum class LEIR_API SizePolicy {
    Fixed,   ///< Size from Rect2D (anchor-resolved).
    Fill,    ///< Fill available space.
    Grow,    ///< Grow to fill, respecting GetMinSize().
    Content, ///< Size from GetContentSize().
};

/**
 * @brief Base UI element: rectangle, layout, parent/children, input and clipping.
 * @ingroup UI
 * @details Owns its children (deleted in the destructor). Supports Free/Row/Column
 *  layouts, anchor-based positioning, clipping (scissor), hit-testing and input
 *  dispatch (OnPointer* / OnKeyDown / OnScroll). Layout is computed via ComputeLayout
 *  with an absolute parentOffset that is added to m_ComputedRect.
 */
class LEIR_API UIElement {
public:
    /**
     * @brief Constructs an element with default rect, Free layout and Fixed size.
     */
    UIElement();

    /**
     * @brief Destroys the element and its owned children.
     */
    virtual ~UIElement();

    /**
     * @brief Returns the mutable anchor rectangle (anchor-relative).
     * @return Reference to m_Rect.
     */
    Rect2D& GetRect() { return m_Rect; }

    /**
     * @brief Returns the anchor rectangle (anchor-relative).
     * @return Const reference to m_Rect.
     */
    const Rect2D& GetRect() const { return m_Rect; }

    /**
     * @brief Sets the layout mode for children.
     * @param[in] mode Free, Row or Column.
     */
    void SetLayoutMode(LayoutMode mode) { m_LayoutMode = mode; }

    /**
     * @brief Returns the layout mode.
     * @return Current LayoutMode.
     */
    LayoutMode GetLayoutMode() const { return m_LayoutMode; }

    /**
     * @brief Sets the size policy within the parent.
     * @param[in] policy Fixed, Fill, Grow or Content.
     */
    void SetSizePolicy(SizePolicy policy) { m_SizePolicy = policy; }

    /**
     * @brief Returns the size policy.
     * @return Current SizePolicy.
     */
    SizePolicy GetSizePolicy() const { return m_SizePolicy; }

    /**
     * @brief Sets padding (left, top, right, bottom) for Row/Column layouts.
     * @param[in] left Left padding.
     * @param[in] top Top padding.
     * @param[in] right Right padding.
     * @param[in] bottom Bottom padding.
     */
    void SetPadding(float left, float top, float right, float bottom);

    /**
     * @brief Sets spacing between children in Row/Column layouts.
     * @param[in] spacing Spacing in logical pixels.
     */
    void SetSpacing(float spacing) { m_Spacing = spacing; }

    /**
     * @brief Returns the top padding (for Row/Column layouts).
     * @return Top padding in logical pixels.
     */
    float GetPaddingTop() const { return m_Padding[1]; }

    /**
     * @brief Sets the pivot (anchor origin) of the rect.
     * @param[in] pivot Pivot in [0,1]×[0,1].
     */
    void SetPivot(const Vector2& pivot) { m_Rect.pivot = pivot; }

    /**
     * @brief Returns the pivot.
     * @return Pivot vector.
     */
    const Vector2& GetPivot() const { return m_Rect.pivot; }

    /**
     * @brief Sets the debug/display name of the element.
     * @param[in] name Name (used by UIRenderer overlay routing, etc.).
     */
    void SetName(const std::string& name) { m_Name = name; }

    /**
     * @brief Returns the element name.
     * @return Name string.
     */
    const std::string& GetName() const { return m_Name; }

    /**
     * @brief Sets the parent of this element (reparents, detaching from old parent).
     * @param[in] parent New parent, or nullptr to detach.
     */
    void SetParent(UIElement* parent);

    /**
     * @brief Returns the parent element.
     * @return Parent pointer or nullptr if root.
     */
    UIElement* GetParent() const { return m_Parent; }

    /**
     * @brief Adds a child (auto-reparents; appends at the end).
     * @param[in] child Child to add (must not be null).
     */
    void AddChild(UIElement* child);

    /**
     * @brief Inserts a child at a specific position (used to reorder tabs in a dock tab bar).
     * @details Auto-reparents like AddChild; the index is clamped to [0, size()].
     * @param[in] child Child to insert.
     * @param[in] index Position in m_Children.
     */
    void InsertChildAt(UIElement* child, size_t index);

    /**
     * @brief Removes a child (does not delete it).
     * @param[in] child Child to remove.
     */
    void RemoveChild(UIElement* child);

    /**
     * @brief Returns the children vector.
     * @return Const reference to m_Children.
     */
    const std::vector<UIElement*>& GetChildren() const { return m_Children; }

    /**
     * @brief Whether this element owns the child for subtree-delete purposes.
     * @details Subtree-delete helpers use this to decide whether they may delete
     *  `child` themselves or must leave it for this element's destructor.
     *  Composite widgets (ScrollView, UITextArea, UIScrollbar) own some internal
     *  children and delete them in their destructor. Default (plain containers): false.
     * @param[in] child Child to query.
     * @return True if owned by this element.
     */
    virtual bool OwnsChild(const UIElement* child) const { (void)child; return false; }

    /**
     * @brief Returns the computed absolute rectangle from the last layout pass.
     * @return Computed rect (x,y,w,h) in logical pixels.
     */
    const Vector4& GetComputedRect() const { return m_ComputedRect; }

    /**
     * @brief Returns the rectangle used for pointer hit-testing.
     * @details Defaults to the computed rect. Elements may override this to
     *  expand (or shrink) the region that receives pointer events without
     *  changing what is drawn — e.g. UIWindow uses it to make a transparent
     *  ring outside its visual rect capture resize/cursor events.
     * @return Hit rect (x,y,w,h) in logical pixels.
     */
    virtual Vector4 GetHitRect() const { return m_ComputedRect; }

    /**
     * @brief Sets the tint color (modulates background/text).
     * @param[in] color RGBA color.
     */
    void SetColor(const Vector4& color) { m_Color = color; }

    /**
     * @brief Returns the tint color.
     * @return Current color.
     */
    const Vector4& GetColor() const { return m_Color; }

    /**
     * @brief Sets active state (inactive elements are not rendered nor hit-tested).
     * @param[in] active Active flag.
     */
    void SetActive(bool active) { m_Active = active; }

    /**
     * @brief Returns whether the element is active.
     * @return True if active.
     */
    bool IsActive() const { return m_Active; }

    /**
     * @brief Marks the element for the top overlay batch (drawn last, above viewports).
     * @param[in] on True to route to the overlay batch.
     */
    void SetOverlayLayer(bool on) { m_IsOverlay = on; }

    /**
     * @brief Whether the element is routed to the overlay batch.
     * @return True if overlay.
     */
    bool IsOverlayLayer() const { return m_IsOverlay; }

    /**
     * @brief Enables clipping: the element's computed rect becomes a clip region for descendants.
     * @param[in] clip True to enable clipping.
     */
    void SetClip(bool clip) { m_Clip = clip; }

    /**
     * @brief Returns whether clipping is enabled.
     * @return True if this element clips its descendants.
     */
    bool IsClipEnabled() const { return m_Clip; }

    /**
     * @brief Sets whether the element participates in hit-testing.
     * @param[in] hit True to be hit-testable.
     */
    void SetHitTestable(bool hit) { m_HitTestable = hit; }

    /**
     * @brief Returns hit-testable flag.
     * @return True if hit-testable.
     */
    bool IsHitTestable() const { return m_HitTestable; }

    /**
     * @brief Returns the minimum size for layout (clamping, Fill/Grow/Content).
     * @return Minimum size in logical pixels.
     */
    virtual Vector2 GetMinSize() const;

    /**
     * @brief Overrides the minimum size for layout.
     * @param[in] minSize Minimum size to enforce (logical pixels).
     */
    void SetMinSize(const Vector2& minSize) { m_MinSizeOverride = minSize; }

    /**
     * @brief Whether a minimum size override is set.
     * @return True if SetMinSize was called.
     */
    bool HasMinSizeOverride() const { return m_MinSizeOverride.has_value(); }

    /**
     * @brief Returns the content size for Content policy (e.g. text extent).
     * @return Content size, or {0,0} by default.
     */
    virtual Vector2 GetContentSize() const;

    /**
     * @brief Lays out this element and children given an available size.
     * @details parentOffset is the absolute position of the parent: it is ADDED to
     *  this element's m_ComputedRect (never accumulated into m_Rect.offset), so the
     *  whole tree ends up with absolute positions without mutating anchor-relative offsets.
     * @param[in] availableSize Available size from the parent (logical pixels).
     * @param[in] parentOffset Absolute position of the parent (default {0,0}).
     */
    virtual void ComputeLayout(const Vector2& availableSize,
        const Vector2& parentOffset = Vector2(0.0f, 0.0f));

    /**
     * @brief Finds a direct or indirect child by name (depth-first).
     * @param[in] name Name to search.
     * @return Matching element or nullptr.
     */
    UIElement* FindChildByName(const std::string& name);

    /**
     * @brief Called when the pointer enters the element's hit area.
     * @param[in] pos Pointer position (logical pixels).
     */
    virtual void OnPointerEnter(const Vector2& pos) {}

    /**
     * @brief Called when the pointer leaves the element.
     */
    virtual void OnPointerExit() {}

    /**
     * @brief Called on pointer move while hovered or captured.
     * @param[in] pos Pointer position.
     */
    virtual void OnPointerMove(const Vector2& pos) {}

    /**
     * @brief Called on pointer press.
     * @param[in] pos Pointer position.
     * @return True if consumed (stops propagation to ancestors).
     */
    virtual bool OnPointerDown(const Vector2& pos) { return false; }

    /**
     * @brief Called on pointer release.
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    virtual bool OnPointerUp(const Vector2& pos) { return false; }

    /**
     * @brief Called for text input (codepoint) when focused.
     * @param[in] codepoint Unicode codepoint.
     * @return True if consumed.
     */
    virtual bool OnTextInput(uint32_t codepoint) { return false; }

    /**
     * @brief Called for key down when focused.
     * @param[in] key Key code (GLFW-compatible).
     * @return True if consumed.
     */
    virtual bool OnKeyDown(int key) { return false; }

    /**
     * @brief Called when the element gains focus.
     */
    virtual void OnFocus() {}

    /**
     * @brief Called when the element loses focus.
     */
    virtual void OnBlur() {}

    /**
     * @brief Called on mouse wheel.
     * @details delta is in scroll lines (positive = scroll up/away).
     * @param[in] delta Scroll delta.
     * @return True to consume the scroll (stop propagation to ancestors).
     */
    virtual bool OnScroll(float delta) { return false; }

    /**
     * @brief Sets hovered state (called by UICanvas).
     * @param[in] h Hovered flag.
     */
    void SetHovered(bool h) { m_Hovered = h; }

    /**
     * @brief Returns hovered state.
     * @return True if hovered.
     */
    bool IsHovered() const { return m_Hovered; }

protected:
    /**
     * @brief Returns the minimum size override if set (for derived GetMinSize).
     * @return Optional override value.
     */
    const std::optional<Vector2>& GetMinSizeOverride() const { return m_MinSizeOverride; }

    /**
     * @brief Hook called after ComputeLayout finished for this element.
     */
    virtual void OnLayoutComputed() {}

    Rect2D m_Rect;                                         ///< Anchor rectangle (anchor-relative).
    LayoutMode m_LayoutMode = LayoutMode::Free;            ///< Layout mode for children.
    SizePolicy m_SizePolicy = SizePolicy::Fixed;           ///< Size policy within parent.
    float m_Padding[4] = {0.0f, 0.0f, 0.0f, 0.0f};           ///< Padding for Row/Column.
    float m_Spacing = 0.0f;                                 ///< Spacing for Row/Column.
    Vector4 m_ComputedRect = {0.0f, 0.0f, 0.0f, 0.0f};       ///< Computed absolute rect (logical).
    Vector4 m_Color = {1.0f, 1.0f, 1.0f, 1.0f};              ///< Tint color.
    bool m_Active = true;                                   ///< Active flag.
    bool m_Hovered = false;                                 ///< Hovered state.
    bool m_IsOverlay = false;                               ///< Overlay batch flag.
    bool m_Clip = false;                                    ///< Clip enabled flag.
    bool m_HitTestable = true;                              ///< Hit-testable flag.
    std::string m_Name;                                     ///< Debug/display name.

private:
    UIElement* m_Parent = nullptr;                          ///< Parent element.
    std::vector<UIElement*> m_Children;                     ///< Owned children.
    std::optional<Vector2> m_MinSizeOverride;               ///< Explicit min size override.
    Vector2 GetNaturalSize() const;
    void ComputeFreeLayout(const Vector2& availableSize, const Vector2& parentOffset);
    void ComputeRowLayout(const Vector2& availableSize, const Vector2& parentOffset);
    void ComputeColumnLayout(const Vector2& availableSize, const Vector2& parentOffset);
};

} // namespace Leir
