#include "LeirEngine/Rendering/Frustum.h"
#include "LeirEngine/Math/Mathf.h"
#include "LeirEngine/Math/Simd.h"

namespace Leir {

void Frustum::SetPlane(int index, float nx, float ny, float nz, float d)
{
    m_NX[index] = nx;
    m_NY[index] = ny;
    m_NZ[index] = nz;
    m_D[index] = d;
    m_Norm[index] = Mathf::Sqrt(nx * nx + ny * ny + nz * nz);
}

void Frustum::Extract(const Matrix4x4& vp)
{
    // Gribb-Hartmann: each frustum plane is a row combination of the
    // clip matrix. Inside = signed distance >= 0 for all 6 planes.
    auto set = [&](int a, int b, bool add, int out) {
        const float s = add ? 1.0f : -1.0f;
        SetPlane(out, vp(a, 0) + s * vp(b, 0),
                 vp(a, 1) + s * vp(b, 1),
                 vp(a, 2) + s * vp(b, 2),
                 vp(a, 3) + s * vp(b, 3));
    };
    set(3, 0, true, 0);  // left
    set(3, 0, false, 1); // right
    set(3, 1, true, 2);  // bottom
    set(3, 1, false, 3); // top
    set(3, 2, true, 4);  // near
    set(3, 2, false, 5); // far
    m_Count = 6;
}

} // namespace Leir