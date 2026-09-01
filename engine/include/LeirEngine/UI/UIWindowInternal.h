#pragma once

/**
 * @file UIWindowInternal.h
 * @brief Internal (embedded) window living inside an existing UICanvas.
 * @ingroup UI
 *
 * The embedded-mode window: it renders its own chrome (title bar, min/max/close
 * buttons, resize borders, drag-to-move, modal overlay) drawn by UIRenderer
 * directly into the host canvas — no OS window. This is the mode used on
 * platforms without OS windows (mobile/web) and for floating windows inside
 * the editor.
 *
 * Usage (editor):
 *   auto* win = new Leir::UIWindowInternal("Test");
 *   win->SetContent(myContent);
 *   win->ShowIn(m_Canvas.get());          // non-modal floating window
 *   win->ShowModalIn(m_Canvas.get());     // modal (blocks the canvas)
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIWindow.h"
#include "LeirEngine/Math/Vector2.h"

namespace Leir {

class UICanvas;

/**
 * @brief Embedded window that lives inside an existing canvas.
 * @ingroup UI
 */
class LEIR_API UIWindowInternal : public UIWindow {
public:
    /**
     * @brief Constructs an internal window (no native window, not shown yet).
     * @param[in] title Window title.
     */
    explicit UIWindowInternal(const std::string& title = "Window");

    /**
     * @brief Destroys the window, its chrome and content.
     */
    ~UIWindowInternal() override;

    /**
     * @brief Shows the window floating inside the given canvas (non-modal).
     * @details Adds the window to the canvas (if not already) and runs the
     *  base Show() (chrome + active + layout + bring-to-front).
     * @param[in] canvas Host canvas.
     */
    void ShowIn(UICanvas* canvas);

    /**
     * @brief Shows the window as modal inside the given canvas.
     * @details Builds the semi-transparent overlay below the window and blocks
     *  input to the rest of the canvas.
     * @param[in] canvas Host canvas.
     */
    void ShowModalIn(UICanvas* canvas);
};

} // namespace Leir