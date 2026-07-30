#pragma once
#include "Vector3.h"
#include <glm/gtc/type_ptr.hpp>
#include <array>

namespace Leir {

struct Matrix3x3 {
    std::array<float, 9> m = {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f
    };

    Matrix3x3() = default;
    explicit Matrix3x3(const std::array<float, 9>& values) : m(values) {}
    explicit Matrix3x3(const glm::mat3& mat) {
        const float* d = glm::value_ptr(mat);
        std::copy(d, d + 9, m.begin());
    }

    operator glm::mat3() const { return glm::make_mat3(m.data()); }

    static Matrix3x3 FromGLM(const glm::mat3& mat) { return Matrix3x3(mat); }

    static Matrix3x3 Identity() { return Matrix3x3(); }

    float& operator()(int row, int col) { return m[row + col * 3]; }
    const float& operator()(int row, int col) const { return m[row + col * 3]; }

    Matrix3x3 operator*(const Matrix3x3& other) const;
    Vector3 operator*(const Vector3& v) const;

    Matrix3x3 Inverse() const;
    Matrix3x3 Transpose() const;

    float* Data() { return m.data(); }
    const float* Data() const { return m.data(); }
};

inline Matrix3x3 Matrix3x3::operator*(const Matrix3x3& other) const {
    glm::mat3 a = glm::make_mat3(m.data());
    glm::mat3 b = glm::make_mat3(other.m.data());
    return Matrix3x3(a * b);
}

inline Vector3 Matrix3x3::operator*(const Vector3& v) const {
    glm::mat3 mat = glm::make_mat3(m.data());
    glm::vec3 gv(v.x, v.y, v.z);
    glm::vec3 r = mat * gv;
    return Vector3(r.x, r.y, r.z);
}

inline Matrix3x3 Matrix3x3::Inverse() const {
    glm::mat3 mat = glm::make_mat3(m.data());
    return Matrix3x3(glm::inverse(mat));
}

inline Matrix3x3 Matrix3x3::Transpose() const {
    glm::mat3 mat = glm::make_mat3(m.data());
    return Matrix3x3(glm::transpose(mat));
}

} // namespace Leir