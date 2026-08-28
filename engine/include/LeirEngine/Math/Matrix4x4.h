#pragma once
#include "Vector3.h"
#include "Vector4.h"
#include "Quaternion.h"
#include "Mathf.h"
#include "Simd.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <array>

namespace Leir {

struct Matrix4x4 {
    std::array<float, 16> m = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    Matrix4x4() = default;
    explicit Matrix4x4(const std::array<float, 16>& values) : m(values) {}
    Matrix4x4(const glm::mat4& mat) {
        const float* d = glm::value_ptr(mat);
        std::copy(d, d + 16, m.begin());
    }

    operator glm::mat4() const { return glm::make_mat4(m.data()); }

    static Matrix4x4 FromGLM(const glm::mat4& mat) { return Matrix4x4(mat); }

    static Matrix4x4 Identity() { return Matrix4x4(); }

    // Access
    float& operator()(int row, int col) { return m[row + col * 4]; }
    const float& operator()(int row, int col) const { return m[row + col * 4]; }

    Matrix4x4 operator*(const Matrix4x4& other) const;
    Vector4 operator*(const Vector4& v) const;
    Vector3 MultiplyPoint(const Vector3& point) const;
    Vector3 MultiplyVector(const Vector3& vec) const;
    Vector3 MultiplyPoint3x4(const Vector3& point) const;

    // SIMD 4x4 multiply (Fase 2): C = A*B computed per output column with
    // splat+FMA over A's columns. Equivalent to glm's scalar mat4*mat4.
    static Matrix4x4 MultiplySimd(const Matrix4x4& a, const Matrix4x4& b) {
        Matrix4x4 c;
        for (int j = 0; j < 4; ++j) {
            Mathf::Simd4f acc = Mathf::SimdMul(Mathf::SimdLoad(&a.m[0]), Mathf::SimdSplat(b(0, j)));
            for (int k = 1; k < 4; ++k)
                acc = Mathf::SimdFma(Mathf::SimdLoad(&a.m[k * 4]), Mathf::SimdSplat(b(k, j)), acc);
            Mathf::SimdStore(&c.m[j * 4], acc);
        }
        return c;
    }

    Matrix4x4 Inverse() const;
    Matrix4x4 Transpose() const;

    // True when every element is finite (no inf/NaN). A singular matrix (e.g.
    // a zero-scaled axis) produces a non-finite inverse via glm::inverse.
    bool IsFinite() const {
        for (float v : m)
            if (!Mathf::IsFinite(v)) return false;
        return true;
    }

    float* Data() { return m.data(); }
    const float* Data() const { return m.data(); }

    // Static construction
    static Matrix4x4 TRS(const Vector3& pos, const Quaternion& rot, const Vector3& scale);
    static Matrix4x4 Perspective(float fovDeg, float aspect, float near, float far);
    static Matrix4x4 Ortho(float left, float right, float bottom, float top, float near, float far);
    static Matrix4x4 LookAt(const Vector3& from, const Vector3& to, const Vector3& up = Vector3::Up());
    static Matrix4x4 Translate(const Vector3& translation);
    static Matrix4x4 Rotate(const Quaternion& rotation);
    static Matrix4x4 Scale(const Vector3& scale);
};

inline Matrix4x4 Matrix4x4::operator*(const Matrix4x4& other) const {
    glm::mat4 a = glm::make_mat4(m.data());
    glm::mat4 b = glm::make_mat4(other.m.data());
    glm::mat4 result = a * b;
    return Matrix4x4(result);
}

inline Vector4 Matrix4x4::operator*(const Vector4& v) const {
    glm::mat4 mat = glm::make_mat4(m.data());
    glm::vec4 gv(v.x, v.y, v.z, v.w);
    glm::vec4 result = mat * gv;
    return Vector4(result.x, result.y, result.z, result.w);
}

inline Vector3 Matrix4x4::MultiplyPoint(const Vector3& point) const {
    return MultiplyPoint3x4(point);
}

inline Vector3 Matrix4x4::MultiplyVector(const Vector3& vec) const {
    glm::mat4 mat = glm::make_mat4(m.data());
    glm::vec4 gv(vec.x, vec.y, vec.z, 0.0f);
    glm::vec4 result = mat * gv;
    return Vector3(result.x, result.y, result.z);
}

inline Vector3 Matrix4x4::MultiplyPoint3x4(const Vector3& point) const {
    glm::mat4 mat = glm::make_mat4(m.data());
    glm::vec4 gv(point.x, point.y, point.z, 1.0f);
    glm::vec4 result = mat * gv;
    return Vector3(result.x / result.w, result.y / result.w, result.z / result.w);
}

inline Matrix4x4 Matrix4x4::Inverse() const {
    glm::mat4 mat = glm::make_mat4(m.data());
    glm::mat4 inv = glm::inverse(mat);
    return Matrix4x4(inv);
}

inline Matrix4x4 Matrix4x4::Transpose() const {
    glm::mat4 mat = glm::make_mat4(m.data());
    glm::mat4 tr = glm::transpose(mat);
    return Matrix4x4(tr);
}

inline Matrix4x4 Matrix4x4::TRS(const Vector3& pos, const Quaternion& rot, const Vector3& scale) {
    glm::mat4 mat = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, pos.y, pos.z));
    mat *= glm::mat4_cast(glm::quat(rot.w, rot.x, rot.y, rot.z));
    mat = glm::scale(mat, glm::vec3(scale.x, scale.y, scale.z));
    return Matrix4x4(mat);
}

inline Matrix4x4 Matrix4x4::Perspective(float fovDeg, float aspect, float near, float far) {
    glm::mat4 mat = glm::perspective(glm::radians(fovDeg), aspect, near, far);
    return Matrix4x4(mat);
}

inline Matrix4x4 Matrix4x4::Ortho(float left, float right, float bottom, float top, float near, float far) {
    glm::mat4 mat = glm::ortho(left, right, bottom, top, near, far);
    return Matrix4x4(mat);
}

inline Matrix4x4 Matrix4x4::LookAt(const Vector3& from, const Vector3& to, const Vector3& up) {
    glm::mat4 mat = glm::lookAt(
        glm::vec3(from.x, from.y, from.z),
        glm::vec3(to.x, to.y, to.z),
        glm::vec3(up.x, up.y, up.z));
    return Matrix4x4(mat);
}

inline Matrix4x4 Matrix4x4::Translate(const Vector3& translation) {
    glm::mat4 mat = glm::translate(glm::mat4(1.0f), glm::vec3(translation.x, translation.y, translation.z));
    return Matrix4x4(mat);
}

inline Matrix4x4 Matrix4x4::Rotate(const Quaternion& rotation) {
    glm::mat4 mat = glm::mat4_cast(glm::quat(rotation.w, rotation.x, rotation.y, rotation.z));
    return Matrix4x4(mat);
}

inline Matrix4x4 Matrix4x4::Scale(const Vector3& scale) {
    glm::mat4 mat = glm::scale(glm::mat4(1.0f), glm::vec3(scale.x, scale.y, scale.z));
    return Matrix4x4(mat);
}

} // namespace Leir