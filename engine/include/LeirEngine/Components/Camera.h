#pragma once

#include "LeirEngine/Core/Component.h"
#include <glm/glm.hpp>

namespace Leir {

class LEIR_API Camera : public Component {
public:
    Camera() = default;

    void SetPerspective(float fovDegrees, float aspect, float nearPlane, float farPlane);
    void SetOrthographic(float size, float aspect, float nearPlane, float farPlane);

    void SetViewMatrix(const glm::mat4& view) { m_ViewMatrix = view; }
    void RecalculateViewMatrix();

    glm::mat4 GetViewMatrix() const { return m_ViewMatrix; }
    glm::mat4 GetProjectionMatrix() const { return m_ProjectionMatrix; }
    glm::mat4 GetViewProjectionMatrix() const { return m_ProjectionMatrix * m_ViewMatrix; }

    bool IsOrthographic() const { return m_Orthographic; }
    float GetFOV() const { return m_FOV; }
    float GetAspect() const { return m_Aspect; }
    float GetNear() const { return m_Near; }
    float GetFar() const { return m_Far; }

    void SetPrimary(bool primary) { m_Primary = primary; }
    bool IsPrimary() const { return m_Primary; }

private:
    glm::mat4 m_ViewMatrix{1.0f};
    glm::mat4 m_ProjectionMatrix{1.0f};

    bool m_Orthographic = false;
    float m_FOV = 60.0f;
    float m_Aspect = 16.0f / 9.0f;
    float m_Near = 0.01f;
    float m_Far = 1000.0f;
    float m_OrthoSize = 10.0f;
    bool m_Primary = false;
};

} // namespace Leir
