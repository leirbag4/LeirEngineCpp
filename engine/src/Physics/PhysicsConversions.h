#pragma once

#include <LeirEngine/Math/Vector3.h>
#include <LeirEngine/Math/Quaternion.h>

#include <Jolt/Jolt.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Math/Quat.h>

namespace Leir {
namespace PhysicsConv {

inline JPH::Vec3 ToJolt(const Vector3& v) {
    return { v.x, v.y, v.z };
}

inline Vector3 ToGLM(const JPH::Vec3& v) {
    return { v.GetX(), v.GetY(), v.GetZ() };
}

inline JPH::Quat ToJolt(const Quaternion& q) {
    return { q.x, q.y, q.z, q.w };
}

inline Quaternion ToGLM(const JPH::Quat& q) {
    return { q.GetX(), q.GetY(), q.GetZ(), q.GetW() };
}

} // namespace PhysicsConv
} // namespace Leir
