#include "LeirEngine/Core/UUID.h"

#include <random>
#include <atomic>

namespace Leir {

static std::atomic<uint64_t> s_NextUUID{0};

static uint64_t GenerateUUID()
{
    static std::mt19937_64 rng(std::random_device{}());
    uint64_t value = rng();
    // Mix in a counter to guarantee uniqueness within this process
    value = (value & ~0xFFFFULL) | (s_NextUUID.fetch_add(1) & 0xFFFFULL);
    return value;
}

UUID::UUID()
    : m_Value(GenerateUUID())
{
}

UUID::UUID(uint64_t value)
    : m_Value(value)
{
}

} // namespace Leir
