#pragma once
#include "LeirEngine/Input/PointerButton.h"
#include "LeirEngine/Input/InputEvent.h"
#include "LeirEngine/Core/Export.h"
#include <glm/glm.hpp>
#include <vector>

namespace Leir {

struct LEIR_API TouchFinger {
    int id = 0;
    glm::vec2 position{ 0.0f };
    glm::vec2 delta{ 0.0f };
    float pressure = 1.0f;
    bool down = false;
    bool pressed = false;
    bool released = false;
};

struct LEIR_API Touch {
    static const std::vector<TouchFinger>& GetFingers();
    static int GetCount();

    static bool IsDown(int fingerId = 0);
    static bool IsUp(int fingerId = 0);
    static bool WasPressed(int fingerId = 0);
    static bool WasReleased(int fingerId = 0);

    static void ProcessEvent(const PointerEvent& e);
    static void ResetFrame();
};

} // namespace Leir
