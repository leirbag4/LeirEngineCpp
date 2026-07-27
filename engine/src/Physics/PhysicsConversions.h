#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Math/Quat.h>

namespace Leir {
namespace PhysicsConv {

inline JPH::Vec3 ToJolt(const glm::vec3& v) {
    return { v.x, v.y, v.z };
}

inline glm::vec3 ToGLM(const JPH::Vec3& v) {
    return { v.GetX(), v.GetY(), v.GetZ() };
}

inline JPH::Quat ToJolt(const glm::quat& q) {
    return { q.x, q.y, q.z, q.w };
}

inline glm::quat ToGLM(const JPH::Quat& q) {
    return { q.GetW(), q.GetX(), q.GetY(), q.GetZ() };
}

} // namespace PhysicsConv
} // namespace Leir
