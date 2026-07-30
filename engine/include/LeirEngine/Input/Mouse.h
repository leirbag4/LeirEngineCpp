#pragma once
#include "LeirEngine/Input/PointerButton.h"
#include "LeirEngine/Input/InputEvent.h"
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector2.h"

namespace Leir {

struct LEIR_API Mouse {
    static bool IsDown(PointerButton btn = PointerButton::Left);
    static bool IsUp(PointerButton btn = PointerButton::Left);
    static bool WasPressed(PointerButton btn = PointerButton::Left);
    static bool WasReleased(PointerButton btn = PointerButton::Left);

    static float GetX();
    static float GetY();
    static Vector2 GetPos();
    static Vector2 GetDelta();
    static float GetScrollDelta();

    static void ProcessEvent(const PointerEvent& e);
    static void ProcessScroll(const ScrollEvent& e);
    static void ResetFrame();
};

} // namespace Leir
