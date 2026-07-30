#include "EditorCamera.h"
#include <LeirEngine/Input/Keyboard.h>
#include <LeirEngine/Input/Mouse.h>

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

Leir::Quaternion EditorCamera::GetRotation() const
{
    return Leir::Quaternion::Euler(m_Pitch, m_Yaw, 0.0f);
}

void EditorCamera::Update(float deltaTime)
{
    bool rightDown = Leir::Mouse::IsDown(Leir::PointerButton::Right);
    bool middleDown = Leir::Mouse::IsDown(Leir::PointerButton::Middle);

    // Pan (middle mouse)
    if (middleDown) {
        auto delta = Leir::Mouse::GetDelta();
        if (delta.x != 0.0f || delta.y != 0.0f) {
            float panSpeed = 0.01f;
            Leir::Vector3 r = GetRight();
            Leir::Vector3 up = Leir::Vector3::Up();
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
    Leir::Vector3 up = Leir::Vector3::Up();

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
