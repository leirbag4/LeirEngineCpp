#include "EditorCamera.h"
#include <LeirEngine/Input/Keyboard.h>
#include <LeirEngine/Input/Mouse.h>
#include <cmath>

EditorCamera::EditorCamera()
{
}

Leir::Vector3 EditorCamera::GetForward() const
{
    Leir::Quaternion rot = GetRotation();
    return (rot * Leir::Vector3::Forward()).Normalized();
}

Leir::Vector3 EditorCamera::GetRight() const
{
    Leir::Quaternion rot = GetRotation();
    return (rot * Leir::Vector3::Right()).Normalized();
}

Leir::Vector3 EditorCamera::GetUp() const
{
    Leir::Quaternion rot = GetRotation();
    return (rot * Leir::Vector3::Up()).Normalized();
}

Leir::Quaternion EditorCamera::GetRotation() const
{
    return Leir::Quaternion::Euler(m_Pitch, m_Yaw, 0.0f);
}

void EditorCamera::SetFromRotation(const Leir::Quaternion& rot)
{
    // Inverse of GetRotation() = Euler(pitch, yaw, 0). GetRotation composes
    // R = Ry(yaw) * Rx(pitch) (roll always 0), so the forward vector is
    //   fwd = R * (0,0,-1) = (-sin(yaw)cos(pitch), sin(pitch), -cos(yaw)cos(pitch))
    // => pitch = asin(fwd.y), yaw = atan2(-fwd.x, -fwd.z). Robust for ANY yaw
    // (no Euler alias: glm::eulerAngles returns (160,80,180) for (-20,100,0),
    // which used to corrupt the camera by injecting roll=180).
    const Leir::Vector3 fwd = rot * Leir::Vector3::Forward();
    m_Yaw = std::atan2(-fwd.x, -fwd.z) * Leir::Mathf::Rad2Deg;
    m_Pitch = std::asin(Leir::Mathf::Clamp(fwd.y, -1.0f, 1.0f)) * Leir::Mathf::Rad2Deg;
    m_Pitch = Leir::Mathf::Clamp(m_Pitch, -89.0f, 89.0f);
}

void EditorCamera::Update(float deltaTime)
{
    bool rightDown = Leir::Mouse::IsDown(Leir::PointerButton::Right);
    bool middleDown = Leir::Mouse::IsDown(Leir::PointerButton::Middle);

    // Pan (middle mouse): move on the camera's UP/RIGHT plane (not world axes).
    if (middleDown) {
        auto delta = Leir::Mouse::GetDelta();
        if (delta.x != 0.0f || delta.y != 0.0f) {
            float panSpeed = 0.01f;
            Leir::Vector3 r = GetRight();
            Leir::Vector3 up = GetUp();
            m_Position -= r * delta.x * panSpeed;
            m_Position += up * delta.y * panSpeed;
        }
    }

    if (!rightDown) return;

    // Yaw/Pitch from mouse delta
    auto delta = Leir::Mouse::GetDelta();
    if (delta.x != 0.0f || delta.y != 0.0f) {
        m_Yaw -= delta.x * 0.5f;
        m_Pitch = Leir::Mathf::Clamp(m_Pitch - delta.y * 0.5f, -89.0f, 89.0f);
    }

    // Movement speed (units per second)
    float speed = 5.0f * deltaTime;
    if (Leir::Keyboard::IsDown(Leir::Key::LeftShift) || Leir::Keyboard::IsDown(Leir::Key::RightShift))
        speed *= 3.0f;

    Leir::Vector3 forward = GetForward();
    Leir::Vector3 right = GetRight();
    Leir::Vector3 up = GetUp(); // camera up (not world up)

    Leir::Vector3 move = Leir::Vector3::Zero();

    if (Leir::Keyboard::IsDown(Leir::Key::W))
        move += forward * speed;
    if (Leir::Keyboard::IsDown(Leir::Key::S))
        move -= forward * speed;
    if (Leir::Keyboard::IsDown(Leir::Key::A))
        move -= right * speed;
    if (Leir::Keyboard::IsDown(Leir::Key::D))
        move += right * speed;
    if (Leir::Keyboard::IsDown(Leir::Key::E))
        move += up * speed;
    if (Leir::Keyboard::IsDown(Leir::Key::Q))
        move -= up * speed;

    m_Position += move;
}
