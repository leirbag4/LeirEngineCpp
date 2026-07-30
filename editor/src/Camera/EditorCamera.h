#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class EditorCamera {
public:
    EditorCamera();

    void Update(float deltaTime);

    glm::vec3 GetPosition() const { return m_Position; }
    float GetYaw() const { return m_Yaw; }
    float GetPitch() const { return m_Pitch; }
    glm::vec3 GetForward() const;
    glm::vec3 GetRight() const;
    glm::quat GetRotation() const;

    void SetPosition(const glm::vec3& pos) { m_Position = pos; }
    void SetYaw(float y) { m_Yaw = y; }
    void SetPitch(float p) { m_Pitch = glm::clamp(p, -89.0f, 89.0f); }

private:
    glm::vec3 m_Position = {0.0f, 2.0f, 4.0f};
    float m_Yaw = 0.0f;
    float m_Pitch = -20.0f;
};
