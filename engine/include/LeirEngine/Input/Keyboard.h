#pragma once
#include "LeirEngine/Input/Key.h"
#include "LeirEngine/Input/InputEvent.h"
#include "LeirEngine/Core/Export.h"
#include <string>

namespace Leir {

struct LEIR_API Keyboard {
    static bool IsDown(Key key);
    static bool IsDown(const KeyCombo& combo);
    static bool IsUp(Key key);
    static bool WasPressed(Key key);
    static bool WasReleased(Key key);

    static Key GetLastPressedKey();
    static std::string GetPressedKeysString();

    static void ProcessEvent(const KeyEvent& e);
    static void ResetFrame();
};

} // namespace Leir
