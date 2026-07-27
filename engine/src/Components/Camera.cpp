#include "LeirEngine/Components/Camera.h"
#include "LeirEngine/Core/CoreObject.h"
#include "LeirEngine/Core/Transform.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Leir {

void Camera::SetPerspective(float fovDegrees, float aspect, float nearPlane, float farPlane)
{
    m_Orthographic = false;
    m_FOV = fovDegrees;
    m_Aspect = aspect;
    m_Near = nearPlane;
    m_Far = farPlane;
    m_ProjectionMatrix = glm::perspective(glm::radians(fovDegrees), aspect, nearPlane, farPlane);
    m_ProjectionMatrix[1][1] *= -1.0f;
}

void Camera::SetOrthographic(float size, float aspect, float nearPlane, float farPlane)
{
    m_Orthographic = true;
    m_OrthoSize = size;
    m_Aspect = aspect;
    m_Near = nearPlane;
    m_Far = farPlane;

    float half = size * 0.5f;
    m_ProjectionMatrix = glm::ortho(-half * aspect, half * aspect, -half, half, nearPlane, farPlane);
}

void Camera::RecalculateViewMatrix()
{
    Transform& transform = GetOwner()->GetTransform();
    m_ViewMatrix = glm::lookAt(
        transform.GetWorldPosition(),
        transform.GetWorldPosition() + transform.GetForward(),
        transform.GetUp()
    );
}

} // namespace Leir
