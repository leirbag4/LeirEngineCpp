#pragma once

/**
 * @file DockSplitter.h
 * @brief Ratio-based splitter bar between dock split children.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/Math/Vector2.h"

namespace Leir {

class DockSplitNode;

/**
 * @brief Ratio-based splitter bar between two children of a DockSplitNode.
 * @ingroup UI
 * @details Dragging adjusts the parent's ratios[] instead of an absolute width.
 */
class LEIR_API DockSplitter : public UIPanel {
public:
    /**
     * @brief Constructs a splitter.
     */
    DockSplitter();

    /**
     * @brief Destroys the splitter.
     */
    ~DockSplitter() override;

    /**
     * @brief Configures the splitter for a node and index.
     * @param[in] node Parent split node.
     * @param[in] index Splitter index in the parent.
     */
    void Configure(DockSplitNode* node, size_t index);

    /**
     * @brief Handles pointer press (starts drag).
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerDown(const Vector2& pos) override;

    /**
     * @brief Handles pointer move (drags and updates ratios).
     * @param[in] pos Pointer position.
     */
    void OnPointerMove(const Vector2& pos) override;

    /**
     * @brief Handles pointer release (ends drag).
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerUp(const Vector2& pos) override;

    /**
     * @brief Called when pointer enters (updates cursor).
     * @param[in] pos Pointer position.
     */
    void OnPointerEnter(const Vector2& pos) override;

    /**
     * @brief Called when pointer leaves.
     */
    void OnPointerExit() override;

    /**
     * @brief Returns minimum size for layout.
     * @return Minimum size (6px strip).
     */
    Vector2 GetMinSize() const override;

private:
    void ApplyCursor();

    DockSplitNode* m_Node = nullptr;                ///< Parent split node.
    size_t m_Index = 0;                             ///< Splitter index.
    bool m_Dragging = false;                        ///< Dragging flag.
    Vector2 m_DragStart = {0.0f, 0.0f};              ///< Drag start position.
};

} // namespace Leir
