#pragma once
#include "LeirEngine/Core/Export.h"
#include <string>

namespace Leir {

class UIElement;

// A dockable panel: the dock tree references DockPanel by pointer; the actual
// content (UIElement subtree) is owned elsewhere (e.g. the editor).
struct LEIR_API DockPanel {
    std::string id;        // stable id used for serialization
    std::string title;     // shown in the tab
    UIElement* content = nullptr;
    bool closeable = false;
    bool active = true;    // open/closed state (persisted)
};

} // namespace Leir
