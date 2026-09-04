#pragma once

/**
 * @file DockPanel.h
 * @brief Dockable panel descriptor: id, title, content and closeable flag.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include <string>

namespace Leir {

class UIElement;

/**
 * @brief Dockable panel descriptor: referenced by the dock tree, content owned elsewhere.
 * @ingroup UI
 */
struct LEIR_API DockPanel {
    std::string id;        ///< Stable id for serialization.
    std::string title;     ///< Display title (tab text).
    UIElement* content = nullptr; ///< Content subtree (owned by caller, e.g. editor).
    bool closeable = false;///< True if closeable via tab X.
    bool active = true;    ///< Open/closed state (persisted).
    bool detached = false; ///< Floating in an external window (not in the dock tree).
};

} // namespace Leir
