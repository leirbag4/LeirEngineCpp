#pragma once
#include <cmath>
#include <algorithm>
#include <type_traits>

namespace Leir {
namespace Mathf {

constexpr float PI = 3.14159265358979323846f;
constexpr float Deg2Rad = PI / 180.0f;
constexpr float Rad2Deg = 180.0f / PI;
constexpr float Epsilon = 1.192092896e-07f;
constexpr float Infinity = std::numeric_limits<float>::infinity();
constexpr float NegativeInfinity = -Infinity;

inline float Sin(float v) { return std::sin(v); }
inline float Cos(float v) { return std::cos(v); }
inline float Tan(float v) { return std::tan(v); }
inline float Asin(float v) { return std::asin(v); }
inline float Acos(float v) { return std::acos(v); }
inline float Atan(float v) { return std::atan(v); }
inline float Atan2(float y, float x) { return std::atan2(y, x); }
inline float Sqrt(float v) { return std::sqrt(v); }
inline float Abs(float v) { return std::fabs(v); }
inline int Abs(int v) { return std::abs(v); }
inline float Sign(float v) { return v < 0.0f ? -1.0f : (v > 0.0f ? 1.0f : 0.0f); }
inline int Sign(int v) { return v < 0 ? -1 : (v > 0 ? 1 : 0); }
inline float Floor(float v) { return std::floor(v); }
inline float Ceil(float v) { return std::ceil(v); }
inline float Round(float v) { return std::round(v); }
inline int FloorToInt(float v) { return (int)std::floor(v); }
inline int CeilToInt(float v) { return (int)std::ceil(v); }
inline int RoundToInt(float v) { return (int)std::round(v); }

template<typename T>
inline T Min(T a, T b) { return a < b ? a : b; }
template<typename T>
inline T Max(T a, T b) { return a > b ? a : b; }

inline float Clamp(float value, float min, float max) {
    return value < min ? min : (value > max ? max : value);
}
inline int Clamp(int value, int min, int max) {
    return value < min ? min : (value > max ? max : value);
}
inline float Clamp01(float value) { return Clamp(value, 0.0f, 1.0f); }

inline float Lerp(float a, float b, float t) {
    return a + (b - a) * Clamp01(t);
}
inline float LerpUnclamped(float a, float b, float t) {
    return a + (b - a) * t;
}
inline float InverseLerp(float a, float b, float value) {
    return (a != b) ? Clamp01((value - a) / (b - a)) : 0.0f;
}

inline float SmoothStep(float from, float to, float t) {
    t = Clamp01(t);
    t = t * t * (3.0f - 2.0f * t);
    return from + (to - from) * t;
}

inline float Approximately(float a, float b) {
    return Abs(b - a) < Max(1e-6f * Max(Abs(a), Abs(b)), Epsilon * 8.0f);
}

inline float MoveTowards(float current, float target, float maxDelta) {
    if (Abs(target - current) <= maxDelta)
        return target;
    return current + Sign(target - current) * maxDelta;
}

inline float Repeat(float t, float length) {
    return Clamp(t - Floor(t / length) * length, 0.0f, length);
}

inline float PingPong(float t, float length) {
    t = Repeat(t, length * 2.0f);
    return length - Abs(t - length);
}

inline float DeltaAngle(float current, float target) {
    float delta = Repeat(target - current, 360.0f);
    if (delta > 180.0f) delta -= 360.0f;
    return delta;
}

inline float SmoothDamp(float current, float target, float& currentVelocity, float smoothTime, float maxSpeed, float deltaTime) {
    smoothTime = Max(0.0001f, smoothTime);
    float omega = 2.0f / smoothTime;
    float x = omega * deltaTime;
    float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
    float change = current - target;
    float maxChange = maxSpeed * smoothTime;
    change = Clamp(change, -maxChange, maxChange);
    float targetTemp = current - change;
    float temp = (currentVelocity + omega * change) * deltaTime;
    currentVelocity = (currentVelocity - omega * temp) * exp;
    float output = targetTemp + (change + temp) * exp;
    if (targetTemp - current > 0.0f == output > targetTemp)
        output = targetTemp;
    return output;
}
inline float SmoothDamp(float current, float target, float& currentVelocity, float smoothTime, float deltaTime) {
    return SmoothDamp(current, target, currentVelocity, smoothTime, Infinity, deltaTime);
}

} // namespace Mathf
} // namespace Leir