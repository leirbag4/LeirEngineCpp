#pragma once

/**
 * @file DockSplitNode.h
 * @brief Split container for the dock tree: positions children by ratios with splitters.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/Dock/DockNode.h"
#include "LeirEngine/Math/Vector2.h"
#include <vector>

namespace Leir {

class DockSplitter;

/**
 * @brief Dock split orientation.
 * @ingroup UI
 */
enum class LEIR_API DockOrientation {
    Horizontal, ///< Children side by side (vertical splitter, ResizeEW).
    Vertical,   ///< Children stacked (horizontal splitter, ResizeNS).
};

/**
 * @brief Split container: positions child nodes by ratios with 6px splitters.
 * @ingroup UI
 */
class LEIR_API DockSplitNode : public DockNode {
public:
    /**
     * @brief Constructs a split node with an orientation.
     * @param[in] orientation Horizontal or Vertical.
     */
    explicit DockSplitNode(DockOrientation orientation);

    /**
     * @brief Destroys the split node and its splitters.
     */
    ~DockSplitNode() override;

    /**
     * @brief Returns orientation.
     * @return Orientation.
     */
    DockOrientation GetOrientation() const { return m_Orientation; }

    /**
     * @brief Returns number of child nodes.
     * @return Count.
     */
    size_t GetNodeCount() const { return m_NodeChildren.size(); }

    /**
     * @brief Returns child node at an index.
     * @param[in] i Index.
     * @return Child node.
     */
    DockNode* GetNode(size_t i) const { return m_NodeChildren[i]; }

    /**
     * @brief Returns ratios (one per child, sum = 1).
     * @return Ratios vector.
     */
    const std::vector<float>& GetRatios() const { return m_Ratios; }

    /**
     * @brief Returns ratio for a specific child.
     * @param[in] child Child node.
     * @return Ratio or 0 if not found.
     */
    float GetRatioForChild(DockNode* child) const;

    /**
     * @brief Adds a child with a ratio.
     * @param[in] child Child to add.
     * @param[in] ratio Ratio (portion of available space).
     */
    void AddNode(DockNode* child, float ratio);

    /**
     * @brief Removes a child.
     * @param[in] child Child to remove.
     */
    void RemoveNode(DockNode* child);

    /**
     * @brief Replaces a child with a new node, preserving its ratio.
     * @param[in] oldNode Node to replace.
     * @param[in] newNode New node.
     * @param[in] ratio Ratio for newNode.
     */
    void ReplaceChild(DockNode* oldNode, DockNode* newNode, float ratio);

    /**
     * @brief Begins a splitter drag (snapshots ratios).
     * @param[in] index Splitter index.
     */
    void BeginSplitterDrag(size_t index);

    /**
     * @brief Drags a splitter by a pixel delta.
     * @param[in] index Splitter index.
     * @param[in] pixelDelta Delta in logical pixels.
     */
    void DragSplitter(size_t index, float pixelDelta);

    /**
     * @brief Returns splitter width.
     * @return Width in logical pixels (6.0).
     */
    float GetSplitterWidth() const { return 6.0f; }

    /**
     * @brief Returns minimum size for layout.
     * @return Minimum size.
     */
    Vector2 GetMinSize() const override;

    /**
     * @brief Lays out children by ratios with splitters.
     * @param[in] availableSize Available size.
     * @param[in] parentOffset Parent absolute offset.
     */
    void ComputeLayout(const Vector2& availableSize, const Vector2& parentOffset = Vector2(0.0f, 0.0f)) override;

private:
    void RebuildSplitters();
    void NormalizeRatios();

    DockOrientation m_Orientation = DockOrientation::Horizontal; ///< Orientation.
    std::vector<DockNode*> m_NodeChildren;                     ///< Child nodes.
    std::vector<float> m_Ratios;                               ///< Ratios per child.
    std::vector<DockSplitter*> m_Splitters;                    ///< Splitter widgets (owned).
    float m_DragStartA = 0.0f;                                 ///< Drag start ratio A.
    float m_DragStartB = 0.0f;                                 ///< Drag start ratio B.
};

} // namespace Leir
