#pragma once

/**
 * @file DockNode.h
 * @brief Base class for dock tree nodes (split containers and panes).
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIPanel.h"

namespace Leir {

/**
 * @brief Type of a dock node.
 * @ingroup UI
 */
enum class LEIR_API DockNodeType {
    Split, ///< Split container (H/V).
    Pane,  ///< Pane with tabs and content.
};

/**
 * @brief Base class for dock tree nodes: split containers and panes.
 * @ingroup UI
 * @details Dock nodes are real UIElements, so they inherit layout, hit-test, input and rendering.
 */
class LEIR_API DockNode : public UIPanel {
public:
    /**
     * @brief Destroys the dock node.
     */
    ~DockNode() override;

    /**
     * @brief Returns the node type.
     * @return Node type.
     */
    DockNodeType GetNodeType() const { return m_NodeType; }

protected:
    /**
     * @brief Constructs a dock node of a type.
     * @param[in] type Node type.
     */
    explicit DockNode(DockNodeType type);

private:
    DockNodeType m_NodeType; ///< Node type.
};

} // namespace Leir
