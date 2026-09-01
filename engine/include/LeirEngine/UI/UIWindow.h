#pragma once

/**
 * @file UIWindow.h
 * @brief Toplevel window base class: title bar, chrome, resize, drag, modal, content hosting.
 * @ingroup UI
 *
 * Abstract base for both internal (embedded) and external (OS-native) windows.
 * UIWindow : UIPanel provides the chrome (title bar, min/max/close buttons, resize
 * borders, drag-to-move) and the host-agnostic API (Show/ShowModal/Close/Hide/
 * BringToFront, WindowResult callbacks, modal overlay, content hosting).
 *
 * Subclasses override OnCreateChrome / OnDestroyChrome to add platform-specific
 * title bar rendering (internal: UIRenderer; external: OS window decorations).
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/UI/UIImage.h"
#include "LeirEngine/UI/UILabel.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Leir {

class Font;
class Texture2D;

/**
 * @brief Result of a modal window interaction.
 * @ingroup UI
 */
enum class LEIR_API WindowResult {
    None = 0,    ///< Not yet closed / result not set.
    Ok,          ///< OK / Accept / Confirm.
    Cancel,      ///< Cancel / Close / Reject.
    Yes,         ///< Yes.
    No,          ///< No.
    Primary,     ///< Primary action (e.g. Save).
    Secondary,   ///< Secondary action (e.g. Don't Save).
};

/**
 * @brief Toplevel window: chrome, content, modal, and host-agnostic lifecycle.
 * @ingroup UI
 *
 * Owns its content subtree (UIElement*). The content is rendered inside the
 * window's client area below the title bar. The chrome is drawn by the
 * subclass (internal mode uses UIRenderer; external mode uses OS decorations).
 *
 * Modal windows show a semi-transparent black overlay over the parent and
 * block input to the parent.
 */
class LEIR_API UIWindow : public UIPanel {
public:
    /**
     * @brief Constructs a window with a default title.
     * @param[in] title Window title.
     */
    UIWindow(const std::string& title = "Window");

    /**
     * @brief Destroys the window, its chrome and content.
     */
    ~UIWindow() override;

    // ---- Lifecycle ----

    /**
     * @brief Opens the window (non-modal).
     * @param[in] parent The parent window, or nullptr for the root.
     */
    virtual void Show(UIWindow* parent = nullptr);

    /**
     * @brief Opens the window as modal (blocks input to the parent).
     * @param[in] parent The parent window (must not be nullptr).
     */
    virtual void ShowModal(UIWindow* parent);

    /**
     * @brief Closes the window and fires OnClosed/OnResult callbacks.
     */
    virtual void Close();

    /**
     * @brief Hides the window without destroying it.
     */
    virtual void Hide();

    /**
     * @brief Brings the window to the top of the Z-order (internal: reorder canvas children).
     */
    virtual void BringToFront();

    // ---- Result (see §7 TODO_WINDOW_SYSTEM.md) ----

    /**
     * @brief Sets the callback invoked when the window closes with a result.
     * @param[in] cb Callback receiving the WindowResult.
     */
    void SetOnResult(std::function<void(WindowResult)> cb) { m_OnResult = std::move(cb); }

    /**
     * @brief Sets the result (called by the window's OK/Cancel buttons).
     * @param[in] r Result value.
     */
    void SetResult(WindowResult r) { m_Result = r; }

    /**
     * @brief Returns the current result.
     * @return WindowResult (None if not yet closed).
     */
    WindowResult GetResult() const { return m_Result; }

    /**
     * @brief Helper: sets OnResult to call the given callback only on Ok.
     * @param[in] cb Callback on Ok.
     */
    void SetOnAccepted(std::function<void()> cb) {
        SetOnResult([cb = std::move(cb)](WindowResult r) { if (r == WindowResult::Ok) cb(); });
    }

    /**
     * @brief Helper: sets OnResult to call the given callback only on Cancel.
     * @param[in] cb Callback on Cancel.
     */
    void SetOnCanceled(std::function<void()> cb) {
        SetOnResult([cb = std::move(cb)](WindowResult r) { if (r == WindowResult::Cancel) cb(); });
    }

    // ---- State ----

    /**
     * @brief Sets the window title.
     * @param[in] title UTF-8 title.
     */
    virtual void SetTitle(const std::string& title);

    /**
     * @brief Sets icons for the chrome min/max/close buttons (PNG via UITextureCache).
     * @details Applied to the internal title-bar buttons. If chrome is not built yet
     *  (SetFont-before-Show style), the icons are stored and applied in OnCreateChrome.
     * @param[in] closeIcon Close button texture.
     * @param[in] minIcon Minimize button texture.
     * @param[in] maxIcon Maximize button texture.
     */
    void SetWindowButtonIcons(std::shared_ptr<Texture2D> closeIcon,
                              std::shared_ptr<Texture2D> minIcon,
                              std::shared_ptr<Texture2D> maxIcon);

    /**
     * @brief Returns the window title.
     * @return Title string.
     */
    const std::string& GetTitle() const { return m_Title; }

    /**
     * @brief Whether the window is visible.
     * @return True if visible.
     */
    bool IsVisible() const { return m_Visible; }

    /**
     * @brief Whether the window is modal.
     * @return True if modal.
     */
    bool IsModal() const { return m_Modal; }

    /**
     * @brief Returns the parent window (set by Show/ShowModal).
     * @return Parent window or nullptr.
     */
    UIWindow* GetParentWindow() const { return m_ParentWindow; }

    // ---- Position & size ----

    /**
     * @brief Sets the window position (top-left in logical pixels).
     * @param[in] pos Position.
     */
    void SetPosition(const Vector2& pos);

    /**
     * @brief Returns the window position.
     * @return Top-left position.
     */
    Vector2 GetPosition() const;

    /**
     * @brief Sets the window size (logical pixels).
     * @param[in] size Width and height.
     */
    void SetSize(const Vector2& size);

    /**
     * @brief Returns the window size.
     * @return Width and height.
     */
    Vector2 GetSize() const;

    /**
     * @brief Sets the minimum size (logical pixels).
     * @param[in] minSize Minimum size.
     */
    void SetMinSize(const Vector2& minSize) { m_MinSize = minSize; }

    /**
     * @brief Sets the maximum size (logical pixels).
     * @param[in] maxSize Maximum size.
     */
    void SetMaxSize(const Vector2& maxSize) { m_MaxSize = maxSize; }

    /**
     * @brief Centers the window on its parent (or on the canvas if no parent).
     */
    void CenterOnParent();

    // ---- Window state ----

    /**
     * @brief Minimizes the window (iconify/dock).
     */
    void Minimize();

    /**
     * @brief Maximizes the window.
     */
    void Maximize();

    /**
     * @brief Restores window from minimized/maximized.
     */
    void Restore();

    /**
     * @brief Whether the window is maximized.
     * @return True if maximized.
     */
    bool IsMaximized() const { return m_Maximized; }

    /**
     * @brief Whether the window is minimized.
     * @return True if minimized.
     */
    bool IsMinimized() const { return m_Minimized; }

    // ---- Flags ----

    /**
     * @brief Sets whether the window can be resized.
     * @param[in] resizable True if resizable.
     */
    void SetResizable(bool resizable) { m_Resizable = resizable; }

    /**
     * @brief Whether the window is resizable.
     * @return Resizable flag.
     */
    bool IsResizable() const { return m_Resizable; }

    /**
     * @brief Sets the invisible resize border thickness (hit area for resize + cursor).
     * @details The cursor changes and resize starts when the pointer is within this
     *  distance of any edge (Windows non-client border). Default 6px.
     * @param[in] size Border thickness in logical pixels (clamped >= 1).
     */
    void SetResizeBorderSize(float size) { m_ResizeBorderSize = std::max(1.0f, size); }

    /**
     * @brief Returns the resize border thickness.
     * @return Border size in logical pixels.
     */
    float GetResizeBorderSize() const { return m_ResizeBorderSize; }

    /**
     * @brief Sets the visible border thickness drawn around the content.
     * @details 0 = no visible border (default). 1 = thin line (Windows-style).
     * @param[in] size Border size in logical pixels (>= 0).
     */
    void SetVisualBorderSize(float size) { m_VisualBorderSize = std::max(0.0f, size); }

    /**
     * @brief Returns the visible border size.
     * @return Border size.
     */
    float GetVisualBorderSize() const { return m_VisualBorderSize; }

    /**
     * @brief Sets the visible border color.
     * @param[in] color RGBA color for the border lines.
     */
    void SetBorderColor(const Vector4& color) { m_BorderColor = color; }

    /**
     * @brief Returns the visible border color.
     * @return Border color.
     */
    const Vector4& GetBorderColor() const { return m_BorderColor; }

    /**
     * @brief Enables/disables the drop shadow behind the window.
     * @param[in] enabled True to show the shadow.
     */
    void SetShadowEnabled(bool enabled) { m_ShadowEnabled = enabled; }

    /**
     * @brief Whether the shadow is enabled.
     * @return True if shadow enabled.
     */
    bool IsShadowEnabled() const { return m_ShadowEnabled; }

    /**
     * @brief Sets the shadow extension (in logical pixels).
     * @param[in] size Shadow size (clamped >= 0).
     */
    void SetShadowSize(float size) { m_ShadowSize = std::max(0.0f, size); }

    /**
     * @brief Returns the shadow size.
     * @return Shadow size.
     */
    float GetShadowSize() const { return m_ShadowSize; }

    /**
     * @brief Shows/hides the title bar.
     * @param[in] hasTitleBar True to show.
     */
    void SetHasTitleBar(bool hasTitleBar) { m_HasTitleBar = hasTitleBar; }

    /**
     * @brief Whether the title bar is visible.
     * @return True if visible.
     */
    bool HasTitleBar() const { return m_HasTitleBar; }

    /**
     * @brief Shows/hides the close button.
     * @param[in] hasClose True to show.
     */
    void SetHasCloseButton(bool hasClose) { m_HasCloseButton = hasClose; }

    /**
     * @brief Shows/hides the minimize button.
     * @param[in] hasMin True to show.
     */
    void SetHasMinimizeButton(bool hasMin) { m_HasMinimizeButton = hasMin; }

    /**
     * @brief Shows/hides the maximize button.
     * @param[in] hasMax True to show.
     */
    void SetHasMaximizeButton(bool hasMax) { m_HasMaximizeButton = hasMax; }

    // ---- Callbacks ----

    /**
     * @brief Sets the callback for when the window is closed.
     * @param[in] cb Callback.
     */
    void SetOnClosed(std::function<void()> cb) { m_OnClosed = std::move(cb); }

    /**
     * @brief Sets the callback for when the window is resized.
     * @param[in] cb Callback receiving (width, height).
     */
    void SetOnResized(std::function<void(int, int)> cb) { m_OnResized = std::move(cb); }

    // ---- Content ----

    /**
     * @brief Sets the content element (takes ownership).
     * @param[in] content Content subtree.
     */
    void SetContent(UIElement* content);

    /**
     * @brief Returns the content element.
     * @return Content pointer or nullptr.
     */
    UIElement* GetContent() const { return m_Content; }

    // ---- Font ----

    /**
     * @brief Sets the font for the title bar.
     * @param[in] font Font pointer.
     */
    void SetFont(Font* font) { m_Font = font; if (m_TitleLabel) m_TitleLabel->SetFont(font); }

    /**
     * @brief Returns the font.
     * @return Font pointer or nullptr.
     */
    Font* GetFont() const { return m_Font; }

    // ---- Input ----

    /**
     * @brief Called on pointer press (used for drag, resize, chrome buttons).
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerDown(const Vector2& pos) override;

    /**
     * @brief Called on pointer move (used for drag/resize).
     * @param[in] pos Pointer position.
     */
    void OnPointerMove(const Vector2& pos) override;

    /**
     * @brief Called when pointer leaves (resets hover + cursor).
     */
    void OnPointerExit() override;

    /**
     * @brief Called on pointer release (ends drag/resize).
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerUp(const Vector2& pos) override;

    /**
     * @brief Returns minimum size (title bar + content min size).
     * @return Minimum size.
     */
    Vector2 GetMinSize() const override;

    /**
     * @brief Whether the window owns the child (for subtree teardown).
     * @param[in] child Child to query.
     * @return True if owned.
     */
    bool OwnsChild(const UIElement* child) const override;

    /**
     * @brief Returns the hit-test rect, expanded by m_ResizeBorderSize.
     * @details The window draws at its visual rect (content fills it edge-to-edge)
     *  but receives pointer events in a transparent ring around it (the resize
     *  border). No expansion when maximized or not resizable.
     * @return Hit rect (x,y,w,h) in logical pixels.
     */
    Vector4 GetHitRect() const override;

protected:
    /**
     * @brief Called after Show()/ShowModal() opens the window.
     */
    virtual void OnShow() {}

    /**
     * @brief Called when the window closes (before result/closed callbacks).
     */
    virtual void OnClose() {}

    /**
     * @brief Called after Show() to build the window chrome.
     * Subclasses override to add title bar, buttons, borders, etc.
     */
    virtual void OnCreateChrome();

    /**
     * @brief Called after Close() to tear down the chrome.
     */
    virtual void OnDestroyChrome();

    /**
     * @brief Called after the content is laid out.
     * Positions the title bar and buttons.
     */
    virtual void OnLayoutChrome();

    /**
     * @brief Re-applies the chrome layout after every canvas layout pass.
     * @details The canvas calls OnLayoutComputed after each UpdateLayout; the
     *  internal chrome (title bar, buttons, content) must follow the window's
     *  computed rect (which moves during drag/resize). External mode has no
     *  internal chrome, so this is a cheap no-op there.
     */
    void OnLayoutComputed() override;

    /**
     * @brief Activates the window: creates chrome, sets active, lays out,
     *  brings to front, calls OnShow. Shared by Show/ShowModal and the
     *  internal subclass (UIWindowInternal::ShowIn/ShowModalIn).
     */
    void Activate();

    // Chrome elements (internal mode)
    UILabel* m_TitleLabel = nullptr;          ///< Title text (owned).
    UIImage* m_CloseButton = nullptr;         ///< Close button image (owned).
    UIImage* m_MinButton = nullptr;           ///< Minimize button image (owned).
    UIImage* m_MaxButton = nullptr;           ///< Maximize button image (owned).
    UIPanel* m_TitleBar = nullptr;            ///< Title bar background (owned).
    UIPanel* m_ModalOverlay = nullptr;        ///< Modal overlay background (owned).

    // Internal state
    bool m_Modal = false;                     ///< Modal flag.
    bool m_Visible = false;                   ///< Visible flag.
    bool m_Maximized = false;                 ///< Maximized state.
    bool m_Minimized = false;                 ///< Minimized state.
    bool m_Resizable = true;                  ///< Resizable flag.
    bool m_HasTitleBar = true;                ///< Title bar visible.
    bool m_HasCloseButton = true;             ///< Close button visible.
    bool m_HasMinimizeButton = true;          ///< Minimize button visible.
    bool m_HasMaximizeButton = true;          ///< Maximize button visible.
    WindowResult m_Result = WindowResult::None; ///< Current result.
    std::string m_Title;                      ///< Window title.
    UIWindow* m_ParentWindow = nullptr;       ///< Parent window.
    UIElement* m_Content = nullptr;           ///< Content subtree (owned).
    Font* m_Font = nullptr;                   ///< Font (not owned).
    Vector2 m_MinSize = {160.0f, 80.0f};      ///< Minimum size.
    Vector2 m_MaxSize = {FLT_MAX, FLT_MAX};   ///< Maximum size.
    Vector2 m_WindowPos = {100.0f, 100.0f};   ///< Window position (logical).
    Vector2 m_WindowSize = {320.0f, 240.0f};  ///< Window size (logical).
    float m_ResizeBorderSize = 6.0f;          ///< Invisible resize border (cursor/hit).
    float m_VisualBorderSize = 0.0f;          ///< Visible border thickness (0 = none).
    Vector4 m_BorderColor = {0.42f, 0.46f, 0.55f, 1.0f}; ///< Visible border color.
    float m_ShadowSize = 14.0f;               ///< Shadow extension (logical).
    bool m_ShadowEnabled = true;              ///< Drop shadow toggle.

    // Drag and resize state
    bool m_Dragging = false;                  ///< Dragging window.
    Vector2 m_DragStart;                      ///< Drag start position (window pos).
    Vector2 m_DragCursorStart;                ///< Drag start cursor position.
    int m_ResizeEdge = 0;                     ///< Resize edge flags (0=none, 1=left, 2=right, 4=top, 8=bottom).
    Vector2 m_ResizeStartSize;                ///< Resize start size.
    Vector2 m_ResizeStartPos;                 ///< Resize start position.
    Vector4 m_RestoredRect;                   ///< Normal rect before maximize (for Restore).

    // Double-click detection (title bar → maximize/restore, Windows-style).
    double m_LastClickTime = -1000.0;         ///< Time of the previous pointer press.
    Vector2 m_LastClickPos = {-1.0f, -1.0f};  ///< Position of the previous pointer press.

    // Callbacks
    std::function<void(WindowResult)> m_OnResult;
    std::function<void()> m_OnClosed;
    std::function<void(int, int)> m_OnResized;

    // Constantes
    static constexpr float kTitleBarHeight = 28.0f;
    static constexpr float kButtonSize = 16.0f;
    static constexpr float kBorderSize = 4.0f;

    // Button textures (PNG icons from UITextureCache, shared_ptr keeps them alive).
    std::shared_ptr<Texture2D> m_CloseIcon;
    std::shared_ptr<Texture2D> m_MinIcon;
    std::shared_ptr<Texture2D> m_MaxIcon;
    // Shadow layers (UIImages behind the window, one quad per layer).
    std::vector<UIImage*> m_ShadowLayers;

    // Shadow helpers
    void CreateShadow();
    void DestroyShadow();
    void UpdateShadowLayout();

    // Returns the hit-test zone for a screen position (like Windows WM_NCHITTEST).
    // 0 = HTCLIENT (inside), 1=left, 2=right, 4=top, 8=bottom, plus combos.
    // Also returns whether the cursor is on the title bar.
    int HitTestZone(const Vector2& pos, bool& onTitleBar) const;
};

} // namespace Leir