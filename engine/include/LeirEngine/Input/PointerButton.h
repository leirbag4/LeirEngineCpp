#pragma once
#include "LeirEngine/Core/Export.h"
#include <cstdint>

namespace Leir {

enum class PointerButton : uint16_t {
    None      = 0,
    Primary   = 1 << 0,
    Secondary = 1 << 1,
    Auxiliary = 1 << 2,
    Extra1    = 1 << 3,
    Extra2    = 1 << 4,
    Extra3    = 1 << 5,
    Extra4    = 1 << 6,
    Extra5    = 1 << 7,
    Extra6    = 1 << 8,
    Extra7    = 1 << 9,
    Extra8    = 1 << 10,
    Left      = Primary,
    Right     = Secondary,
    Middle    = Auxiliary,
};

inline PointerButton operator|(PointerButton a, PointerButton b) {
    return static_cast<PointerButton>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}
inline PointerButton operator&(PointerButton a, PointerButton b) {
    return static_cast<PointerButton>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}
inline PointerButton operator~(PointerButton a) {
    return static_cast<PointerButton>(~static_cast<uint16_t>(a));
}
inline bool Any(PointerButton a) {
    return static_cast<uint16_t>(a) != 0;
}
inline bool Has(PointerButton a, PointerButton b) {
    return (static_cast<uint16_t>(a) & static_cast<uint16_t>(b)) != 0;
}

} // namespace Leir
