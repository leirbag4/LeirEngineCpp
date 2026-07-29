#pragma once
#include "LeirEngine/Core/Export.h"
#include <cstdint>
#include <vector>

namespace Leir {

enum class Key : int32_t {
    Unknown = -1,
    Space = 32,
    Apostrophe = 39,
    Comma = 44,
    Minus = 45,
    Period = 46,
    Slash = 47,
    Alpha0 = 48, Alpha1, Alpha2, Alpha3, Alpha4, Alpha5, Alpha6, Alpha7, Alpha8, Alpha9,
    Semicolon = 59,
    Equal = 61,
    A = 65, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    LeftBracket = 91,
    Backslash = 92,
    RightBracket = 93,
    GraveAccent = 96,
    World1 = 161,
    World2 = 162,
    Escape = 256,
    Enter,
    Tab,
    Backspace,
    Insert,
    Delete,
    Right, Left, Down, Up,
    PageUp, PageDown, Home, End,
    CapsLock = 280,
    ScrollLock,
    NumLock,
    PrintScreen,
    Pause,
    F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    F13 = 310, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24, F25,
    NumPad0 = 320, NumPad1, NumPad2, NumPad3, NumPad4,
    NumPad5, NumPad6, NumPad7, NumPad8, NumPad9,
    NumPadDecimal = 330,
    NumPadDivide = 331,
    NumPadMultiply = 332,
    NumPadSubtract = 333,
    NumPadAdd = 334,
    NumPadEnter = 335,
    NumPadEqual = 336,
    LeftShift = 340,
    LeftControl,
    LeftAlt,
    LeftSuper,
    RightShift,
    RightControl,
    RightAlt,
    RightSuper,
    Menu = 348,
    Last = Menu,
};

struct LEIR_API KeyCombo {
    std::vector<Key> keys;

    KeyCombo() = default;
    KeyCombo(Key k) : keys{ k } {}
    KeyCombo(std::initializer_list<Key> ks) : keys(ks) {}

    void Add(Key k) { keys.push_back(k); }
};

inline KeyCombo operator|(Key a, Key b) { return { a, b }; }
inline KeyCombo operator|(Key a, KeyCombo b) { b.keys.insert(b.keys.begin(), a); return b; }
inline KeyCombo operator|(KeyCombo a, Key b) { a.keys.push_back(b); return a; }

} // namespace Leir
