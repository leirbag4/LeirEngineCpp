#pragma once
#include "Vector2.h"
#include "Vector3.h"
#include <random>
#include <cstdint>

namespace Leir {

class Random {
public:
    Random() : m_Rng(std::random_device{}()) {}
    explicit Random(uint32_t seed) : m_Rng(seed) {}

    void SetSeed(uint32_t seed) { m_Rng.seed(seed); }

    float Range(float min, float max) {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(m_Rng);
    }

    int Range(int min, int max) {
        std::uniform_int_distribution<int> dist(min, max - 1);
        return dist(m_Rng);
    }

    float Value() {
        return Range(0.0f, 1.0f);
    }

    Vector2 InsideUnitCircle() {
        float angle = Range(0.0f, Mathf::PI * 2.0f);
        float r = std::sqrt(Value());
        return {std::cos(angle) * r, std::sin(angle) * r};
    }

    Vector3 InsideUnitSphere() {
        float theta = Range(0.0f, Mathf::PI * 2.0f);
        float phi = std::acos(Range(-1.0f, 1.0f));
        float r = std::pow(Value(), 1.0f / 3.0f);
        return {std::sin(phi) * std::cos(theta) * r,
                std::sin(phi) * std::sin(theta) * r,
                std::cos(phi) * r};
    }

    Vector3 OnUnitSphere() {
        float theta = Range(0.0f, Mathf::PI * 2.0f);
        float phi = std::acos(Range(-1.0f, 1.0f));
        return {std::sin(phi) * std::cos(theta),
                std::sin(phi) * std::sin(theta),
                std::cos(phi)};
    }

    Quaternion Rotation() {
        float u1 = Value(), u2 = Value(), u3 = Value();
        float sqrt = std::sqrt(1.0f - u1);
        return Quaternion(
            sqrt * std::sin(Mathf::PI * 2.0f * u2),
            sqrt * std::cos(Mathf::PI * 2.0f * u2),
            std::sqrt(u1) * std::sin(Mathf::PI * 2.0f * u3),
            std::sqrt(u1) * std::cos(Mathf::PI * 2.0f * u3)
        );
    }

private:
    std::mt19937 m_Rng;
};

} // namespace Leir