#pragma once
#include "LeirEngine/Input/Key.h"
#include "LeirEngine/Input/InputEvent.h"
#include "LeirEngine/Core/Export.h"

namespace Leir {

struct LEIR_API Keyboard {
    static bool IsDown(Key key);
    static bool IsDown(const KeyCombo& combo);
    static bool IsUp(Key key);
    static bool WasPressed(Key key);
    static bool WasReleased(Key key);

    static void ProcessEvent(const KeyEvent& e);
    static void ResetFrame();

private:
    friend struct KeyboardState;
};

struct KeyboardState {
    bool current[384] = {};
    bool previous[384] = {};

    static KeyboardState& Get();
};

} // namespace Leir
