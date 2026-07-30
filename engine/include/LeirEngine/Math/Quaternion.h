#pragma once
#include "Mathf.h"
#include "Vector3.h"
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <cmath>

namespace Leir {

struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    Quaternion() = default;
    constexpr Quaternion(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    Quaternion(const glm::quat& q) : x(q.x), y(q.y), z(q.z), w(q.w) {}

    operator glm::quat() const { return glm::quat(w, x, y, z); }

    static Quaternion FromGLM(const glm::quat& q) { return Quaternion(q); }

    static Quaternion Identity() { return {0.0f, 0.0f, 0.0f, 1.0f}; }

    Quaternion operator*(const Quaternion& q) const;
    Vector3 operator*(const Vector3& v) const;
    Quaternion& operator*=(const Quaternion& q) { *this = *this * q; return *this; }

    bool operator==(const Quaternion& q) const { return x == q.x && y == q.y && z == q.z && w == q.w; }
    bool operator!=(const Quaternion& q) const { return x != q.x || y != q.y || z != q.z || w != q.w; }

    Quaternion Inverse() const;
    Quaternion Normalized() const;
    void Normalize();

    // Static methods
    static Quaternion Euler(float xDeg, float yDeg, float zDeg);
    static Quaternion Euler(const Vector3& eulerDeg);
    static Vector3 ToEuler(const Quaternion& q);
    static Quaternion AngleAxis(float angleDeg, const Vector3& axis);
    static Quaternion LookRotation(const Vector3& forward, const Vector3& up = Vector3::Up());
    static Quaternion FromToRotation(const Vector3& fromDir, const Vector3& toDir);
    static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);
    static Quaternion Lerp(const Quaternion& a, const Quaternion& b, float t);
    static Quaternion RotateTowards(const Quaternion& from, const Quaternion& to, float maxDegreesDelta);
    static float Angle(const Quaternion& a, const Quaternion& b);
    static float Dot(const Quaternion& a, const Quaternion& b);
};

inline Quaternion Quaternion::operator*(const Quaternion& q) const {
    return Quaternion(
        w * q.x + x * q.w + y * q.z - z * q.y,
        w * q.y - x * q.z + y * q.w + z * q.x,
        w * q.z + x * q.y - y * q.x + z * q.w,
        w * q.w - x * q.x - y * q.y - z * q.z
    );
}

inline Vector3 Quaternion::operator*(const Vector3& v) const {
    glm::vec3 gv = glm::quat(w, x, y, z) * glm::vec3(v.x, v.y, v.z);
    return Vector3::FromGLM(gv);
}

inline Quaternion Quaternion::Inverse() const {
    float sq = x * x + y * y + z * z + w * w;
    return sq > 0.0f ? Quaternion(-x / sq, -y / sq, -z / sq, w / sq) : Identity();
}

inline Quaternion Quaternion::Normalized() const {
    float sq = x * x + y * y + z * z + w * w;
    if (sq <= 0.0f) return Identity();
    float inv = 1.0f / std::sqrt(sq);
    return Quaternion(x * inv, y * inv, z * inv, w * inv);
}

inline void Quaternion::Normalize() {
    float sq = x * x + y * y + z * z + w * w;
    if (sq <= 0.0f) { *this = Identity(); return; }
    float inv = 1.0f / std::sqrt(sq);
    x *= inv; y *= inv; z *= inv; w *= inv;
}

inline Quaternion Quaternion::Euler(float xDeg, float yDeg, float zDeg) {
    float h = glm::radians(yDeg) * 0.5f;
    float p = glm::radians(xDeg) * 0.5f;
    float b = glm::radians(zDeg) * 0.5f;
    float ch = std::cos(h), sh = std::sin(h);
    float cp = std::cos(p), sp = std::sin(p);
    float cb = std::cos(b), sb = std::sin(b);
    return Quaternion(
        ch * sp * cb + sh * cp * sb,
        -ch * sp * sb + sh * cp * cb,
        ch * cp * sb - sh * sp * cb,
        ch * cp * cb + sh * sp * sb
    );
}

inline Quaternion Quaternion::Euler(const Vector3& eulerDeg) {
    return Euler(eulerDeg.x, eulerDeg.y, eulerDeg.z);
}

inline Vector3 Quaternion::ToEuler(const Quaternion& q) {
    glm::quat gq(q.w, q.x, q.y, q.z);
    glm::vec3 euler = glm::eulerAngles(gq);
    return Vector3(Mathf::Rad2Deg * euler.x, Mathf::Rad2Deg * euler.y, Mathf::Rad2Deg * euler.z);
}

inline Quaternion Quaternion::AngleAxis(float angleDeg, const Vector3& axis) {
    float rad = glm::radians(angleDeg) * 0.5f;
    float s = std::sin(rad);
    return Quaternion(axis.x * s, axis.y * s, axis.z * s, std::cos(rad));
}

inline Quaternion Quaternion::LookRotation(const Vector3& forward, const Vector3& up) {
    glm::vec3 f = glm::vec3(forward.x, forward.y, forward.z);
    glm::vec3 u = glm::vec3(up.x, up.y, up.z);
    if (glm::length(f) < 1e-6f) return Identity();
    glm::quat q = glm::quatLookAt(glm::normalize(f), glm::normalize(u));
    return Quaternion(q.x, q.y, q.z, q.w);
}

inline Quaternion Quaternion::FromToRotation(const Vector3& fromDir, const Vector3& toDir) {
    glm::vec3 f = glm::vec3(fromDir.x, fromDir.y, fromDir.z);
    glm::vec3 t = glm::vec3(toDir.x, toDir.y, toDir.z);
    f = glm::normalize(f);
    t = glm::normalize(t);
    float dot = glm::clamp(glm::dot(f, t), -1.0f, 1.0f);
    if (dot > 0.9999f) return Identity();
    if (dot < -0.9999f) {
        glm::vec3 axis = glm::cross(f, glm::vec3(0.0f, 1.0f, 0.0f));
        if (glm::length(axis) < 1e-6f) axis = glm::cross(f, glm::vec3(1.0f, 0.0f, 0.0f));
        return AngleAxis(180.0f, Vector3::FromGLM(glm::normalize(axis)));
    }
    glm::vec3 axis2 = glm::cross(f, t);
    float angle = std::acos(dot);
    float s = std::sin(angle * 0.5f);
    return Quaternion(axis2.x * s, axis2.y * s, axis2.z * s, std::cos(angle * 0.5f));
}

inline Quaternion Quaternion::Slerp(const Quaternion& a, const Quaternion& b, float t) {
    t = Mathf::Clamp01(t);
    glm::quat qa(a.w, a.x, a.y, a.z);
    glm::quat qb(b.w, b.x, b.y, b.z);
    glm::quat result = glm::slerp(qa, qb, t);
    return Quaternion(result.x, result.y, result.z, result.w);
}

inline Quaternion Quaternion::Lerp(const Quaternion& a, const Quaternion& b, float t) {
    t = Mathf::Clamp01(t);
    float invT = 1.0f - t;
    float dot = Dot(a, b);
    if (dot < 0.0f) { invT = -invT; }
    Quaternion q(a.x * invT + b.x * t, a.y * invT + b.y * t, a.z * invT + b.z * t, a.w * invT + b.w * t);
    return q.Normalized();
}

inline Quaternion Quaternion::RotateTowards(const Quaternion& from, const Quaternion& to, float maxDegreesDelta) {
    float angle = Angle(from, to);
    if (angle <= 0.0f) return to;
    float t = Mathf::Min(1.0f, maxDegreesDelta / angle);
    return Slerp(from, to, t);
}

inline float Quaternion::Angle(const Quaternion& a, const Quaternion& b) {
    float dot = Mathf::Clamp(Dot(a, b), -1.0f, 1.0f);
    return std::acos(dot) * 2.0f * 57.29577951308232f;
}

inline float Quaternion::Dot(const Quaternion& a, const Quaternion& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

} // namespace Leir