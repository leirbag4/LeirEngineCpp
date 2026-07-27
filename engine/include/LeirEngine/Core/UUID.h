#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Core/Types.h"

namespace Leir {

class LEIR_API UUID {
public:
    UUID();
    explicit UUID(uint64_t value);
    UUID(const UUID&) = default;
    UUID& operator=(const UUID&) = default;

    operator uint64_t() const { return m_Value; }
    bool operator==(const UUID& other) const { return m_Value == other.m_Value; }
    bool operator!=(const UUID& other) const { return m_Value != other.m_Value; }

    uint64_t GetValue() const { return m_Value; }

private:
    uint64_t m_Value;
};

} // namespace Leir

namespace std {

template<>
struct hash<Leir::UUID> {
    size_t operator()(const Leir::UUID& uuid) const {
        return hash<uint64_t>()((uint64_t)uuid);
    }
};

} // namespace std
