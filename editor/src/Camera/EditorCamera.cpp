#include "EditorCamera.h"
#include <LeirEngine/Input/Keyboard.h>
#include <LeirEngine/Input/Mouse.h>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

EditorCamera::EditorCamera()
{
}

glm::vec3 EditorCamera::GetForward() const
{
    glm::quat rot = GetRotation();
    return glm::normalize(rot * glm::vec3(0.0f, 0.0f, -1.0f));
}

glm::vec3 EditorCamera::GetRight() const
{
    glm::quat rot = GetRotation();
    return glm::normalize(rot * glm::vec3(1.0f, 0.0f, 0.0f));
}

glm::quat EditorCamera::GetRotation() const
{
    return glm::quat(glm::vec3(glm::radians(m_Pitch), glm::radians(m_Yaw), 0.0f));
}

void EditorCamera::Update(float deltaTime)
{
    bool rightDown = Leir::Mouse::IsDown(Leir::PointerButton::Right);
    bool middleDown = Leir::Mouse::IsDown(Leir::PointerButton::Middle);

    // Pan (middle mouse) — mover cámara en plano de la vista
    if (middleDown) {
        auto delta = Leir::Mouse::GetDelta();
        if (delta.x != 0.0f || delta.y != 0.0f) {
            float panSpeed = 0.01f;
            glm::vec3 r = GetRight();
            glm::vec3 up(0.0f, 1.0f, 0.0f);
            m_Position -= r * delta.x * panSpeed;
            m_Position += up * delta.y * panSpeed;
        }
    }

    if (!rightDown) return;

    // Yaw/Pitch from mouse delta
    auto delta = Leir::Mouse::GetDelta();
    if (delta.x != 0.0f || delta.y != 0.0f) {
        m_Yaw -= delta.x * 0.5f;
        m_Pitch = glm::clamp(m_Pitch - delta.y * 0.5f, -89.0f, 89.0f);
    }

    // Movement speed (units per second)
    float speed = 5.0f * deltaTime;
    if (Leir::Keyboard::IsDown(Leir::Key::LeftShift) || Leir::Keyboard::IsDown(Leir::Key::RightShift))
        speed *= 3.0f;

    glm::vec3 forward = GetForward();
    glm::vec3 right = GetRight();
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    glm::vec3 move(0.0f);

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
