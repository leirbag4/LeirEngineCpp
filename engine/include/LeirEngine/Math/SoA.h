#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Simd.h"

#include <cstddef>

namespace Leir {
namespace Mathf {

// SoA (structure-of-arrays) column primitives (Fase 2, Incremento 4,
// TODO_HYBRID_ECS.md §5/§10): process a float column 4 elements at a time with
// Simd4f. Contiguous per-field columns let hot systems SIMD-process all the
// positions, matrices, etc. of a component group in one pass.
//
//   SimdAddFloats(dst, a, b, n)  // dst[i] = a[i] + b[i]
//   SimdMulFloats(dst, a, b, n)  // dst[i] = a[i] * b[i]
//   SimdFmaFloats(dst, a, b, c, n) // dst[i] = a[i] * b[i] + c[i]

inline void SimdAddFloats(float* dst, const float* a, const float* b, size_t n)
{
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        SimdStore(dst + i, SimdAdd(SimdLoad(a + i), SimdLoad(b + i)));
    }
    for (; i < n; ++i)
        dst[i] = a[i] + b[i];
}

inline void SimdMulFloats(float* dst, const float* a, const float* b, size_t n)
{
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        SimdStore(dst + i, SimdMul(SimdLoad(a + i), SimdLoad(b + i)));
    }
    for (; i < n; ++i)
        dst[i] = a[i] * b[i];
}

inline void SimdFmaFloats(float* dst, const float* a, const float* b, const float* c, size_t n)
{
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        SimdStore(dst + i, SimdFma(SimdLoad(a + i), SimdLoad(b + i), SimdLoad(c + i)));
    }
    for (; i < n; ++i)
        dst[i] = a[i] * b[i] + c[i];
}

} // namespace Mathf
} // namespace Leir