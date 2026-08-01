#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIPanel.h"

namespace Leir {

enum class LEIR_API DockNodeType {
    Split,
    Pane,
};

// Base class for dock tree nodes (split containers and panes). Dock nodes are
// real UI elements, so they inherit layout, hit-test, input and rendering.
class LEIR_API DockNode : public UIPanel {
public:
    ~DockNode() override;

    DockNodeType GetNodeType() const { return m_NodeType; }

protected:
    explicit DockNode(DockNodeType type);

private:
    DockNodeType m_NodeType;
};

} // namespace Leir
