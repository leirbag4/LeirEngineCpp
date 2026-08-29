#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Matrix4x4.h"
#include "LeirEngine/Math/Mathf.h"
#include "LeirEngine/Math/Simd.h"
#include "LeirEngine/Math/Vector3.h"

namespace Leir {

// View-frustum with SIMD culling (Fase 2 "Render list vectorizado",
// TODO_HYBRID_ECS.md §5): the 6 planes are extracted once from the
// view-projection matrix (Gribb-Hartmann) and stored in SoA groups (2 groups
// of 4 lanes, lane = plane). TestSphere processes one sphere against 4 planes
// at a time with Simd4f (FMA + precomputed |n| for the conservative radius
// term) and a horizontal-min reduction (1 lane extract per group). Inline so
// the per-renderable call vectorizes in the render list build.
class LEIR_API Frustum {
public:
    Frustum() = default;

    // Extracts the 6 clip planes from viewProj (column-major GLM convention).
    void Extract(const Matrix4x4& viewProj);

    // True if the sphere (conservative bound of an AABB/mesh) is inside or
    // intersects the frustum. SIMD across 4 planes per pass.
    bool TestSphere(const Vector3& c, float radius) const
    {
        using namespace Mathf;
        Simd4f cx = SimdSplat(c.x), cy = SimdSplat(c.y), cz = SimdSplat(c.z);
        Simd4f r = SimdSplat(radius);
        for (int g = 0; g < m_Count; g += 4) {
            Simd4f nx = SimdLoad(&m_NX[g]), ny = SimdLoad(&m_NY[g]), nz = SimdLoad(&m_NZ[g]);
            Simd4f d = SimdLoad(&m_D[g]), norm = SimdLoad(&m_Norm[g]);
            Simd4f dist = SimdFma(nx, cx, SimdFma(ny, cy, SimdFma(nz, cz, d)));
            Simd4f t = SimdAdd(dist, SimdMul(norm, r));
            // min over the 4 lanes < 0 → outside that plane → culled.
            if (SimdHMin(t) < 0.0f)
                return false;
        }
        return true;
    }

    // Scalar reference implementation (used by tests/benchmarks).
    bool TestSphereScalar(const Vector3& c, float radius) const
    {
        for (int i = 0; i < m_Count; ++i) {
            float dist = m_NX[i] * c.x + m_NY[i] * c.y + m_NZ[i] * c.z + m_D[i];
            if (dist + m_Norm[i] * radius < 0.0f)
                return false;
        }
        return true;
    }

private:
    void SetPlane(int index, float nx, float ny, float nz, float d);

    // SoA plane storage: for lane i of group g, plane = g + i.
    float m_NX[8] = { 0 }, m_NY[8] = { 0 }, m_NZ[8] = { 0 }, m_D[8] = { 0 };
    float m_Norm[8] = { 0 }; // |n| per plane (precomputed for the radius term)
    int m_Count = 0;
};

} // namespace Leir