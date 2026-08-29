#pragma once

// SIMD wrappers for LeirEngine (Fase 2, TODO_HYBRID_ECS.md §5). Per the Mathf
// rule, ALL SIMD lives inside the Math module — never raw intrinsics elsewhere.
// Simd4f packs 4 floats and is backed by:
//   x86/x64   : SSE2 (baseline on x64; AVX/AVX2 slot in here as a runtime
//               dispatch once the project enables /arch:AVX2)
//   ARM64     : NEON float32x4_t (macOS/Android/iOS)
//   WebAssembly : SIMD128 (v128_t)
//   fallback  : a scalar struct (any other target, e.g. RISC-V)

#include "LeirEngine/Core/Export.h"

#include <cstdint>

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
  #if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86_FP) && _M_IX86_FP >= 2
    #define LEIR_SIMD_SSE2 1
  #endif
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__ARM_NEON)
  #define LEIR_SIMD_NEON 1
#elif defined(__wasm_simd128__)
  #define LEIR_SIMD_WASM 1
#endif

#if !defined(LEIR_SIMD_SSE2) && !defined(LEIR_SIMD_NEON) && !defined(LEIR_SIMD_WASM)
  #define LEIR_SIMD_SCALAR 1
#endif

#if defined(LEIR_SIMD_SSE2)
  #include <emmintrin.h>
#elif defined(LEIR_SIMD_NEON)
  #include <arm_neon.h>
#elif defined(LEIR_SIMD_WASM)
  #include <wasm_simd128.h>
#endif

namespace Leir {
namespace Mathf {

#if defined(LEIR_SIMD_SSE2)
using Simd4f = __m128;
inline Simd4f SimdLoad(const float* p) { return _mm_loadu_ps(p); }
inline void SimdStore(float* p, Simd4f v) { _mm_storeu_ps(p, v); }
inline Simd4f SimdAdd(Simd4f a, Simd4f b) { return _mm_add_ps(a, b); }
inline Simd4f SimdMul(Simd4f a, Simd4f b) { return _mm_mul_ps(a, b); }
inline Simd4f SimdFma(Simd4f a, Simd4f b, Simd4f c) { return _mm_add_ps(_mm_mul_ps(a, b), c); }
inline Simd4f SimdSplat(float f) { return _mm_set1_ps(f); }
inline float SimdGetX(Simd4f v) { return _mm_cvtss_f32(v); }
inline float SimdGetLane(Simd4f v, int lane) { return ((const float*)&v)[lane]; }
inline float SimdHMin(Simd4f v) {
    v = _mm_min_ps(v, _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 0, 3, 2)));
    v = _mm_min_ps(v, _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1)));
    return _mm_cvtss_f32(v);
}

#elif defined(LEIR_SIMD_NEON)
using Simd4f = float32x4_t;
inline Simd4f SimdLoad(const float* p) { return vld1q_f32(p); }
inline void SimdStore(float* p, Simd4f v) { vst1q_f32(p, v); }
inline Simd4f SimdAdd(Simd4f a, Simd4f b) { return vaddq_f32(a, b); }
inline Simd4f SimdMul(Simd4f a, Simd4f b) { return vmulq_f32(a, b); }
inline Simd4f SimdFma(Simd4f a, Simd4f b, Simd4f c) { return vfmaq_f32(c, a, b); }
inline Simd4f SimdSplat(float f) { return vdupq_n_f32(f); }
inline float SimdGetX(Simd4f v) { return vgetq_lane_f32(v, 0); }
inline float SimdGetLane(Simd4f v, int lane) { float f[4]; vst1q_f32(f, v); return f[lane]; }
inline float SimdHMin(Simd4f v) { return vminvq_f32(v); }

#elif defined(LEIR_SIMD_WASM)
using Simd4f = v128_t;
inline Simd4f SimdLoad(const float* p) { return wasm_v128_load(p); }
inline void SimdStore(float* p, Simd4f v) { wasm_v128_store(p, v); }
inline Simd4f SimdAdd(Simd4f a, Simd4f b) { return f32x4_add(a, b); }
inline Simd4f SimdMul(Simd4f a, Simd4f b) { return f32x4_mul(a, b); }
inline Simd4f SimdFma(Simd4f a, Simd4f b, Simd4f c) { return f32x4_add(f32x4_mul(a, b), c); }
inline Simd4f SimdSplat(float f) { return f32x4_splat(f); }
inline float SimdGetX(Simd4f v) { return wasm_f32x4_extract_lane(v, 0); }
inline float SimdGetLane(Simd4f v, int lane) { float f[4]; wasm_v128_store(f, v); return f[lane]; }
inline float SimdHMin(Simd4f v) {
    v = f32x4_min(v, wasm_i32x4_shuffle(v, v, 1, 0, 3, 2));
    v = f32x4_min(v, wasm_i32x4_shuffle(v, v, 2, 3, 0, 1));
    return wasm_f32x4_extract_lane(v, 0);
}

#else // LEIR_SIMD_SCALAR (portable fallback — e.g. RISC-V, or x86 without SSE2)
struct LEIR_API Simd4f {
    float x = 0, y = 0, z = 0, w = 0;
};
inline Simd4f SimdLoad(const float* p) { return {p[0], p[1], p[2], p[3]}; }
inline void SimdStore(float* p, Simd4f v) { p[0] = v.x; p[1] = v.y; p[2] = v.z; p[3] = v.w; }
inline Simd4f SimdAdd(Simd4f a, Simd4f b) { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
inline Simd4f SimdMul(Simd4f a, Simd4f b) { return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w}; }
inline Simd4f SimdFma(Simd4f a, Simd4f b, Simd4f c) { return SimdAdd(SimdMul(a, b), c); }
inline Simd4f SimdSplat(float f) { return {f, f, f, f}; }
inline float SimdGetX(Simd4f v) { return v.x; }
inline float SimdGetLane(Simd4f v, int lane) { return lane == 0 ? v.x : (lane == 1 ? v.y : (lane == 2 ? v.z : v.w)); }
inline float SimdHMin(Simd4f v) { return Min(v.x, Min(v.y, Min(v.z, v.w))); }
#endif

} // namespace Mathf
} // namespace Leir