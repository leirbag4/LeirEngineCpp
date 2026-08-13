#include "LeirEngine/Components/Camera.h"
#include "LeirEngine/Core/CoreObject.h"
#include "LeirEngine/Core/Transform.h"

namespace Leir {

void Camera::SetPerspective(float fovDegrees, float aspect, float nearPlane, float farPlane)
{
    m_Orthographic = false;
    m_FOV = fovDegrees;
    m_Aspect = aspect;
    m_Near = nearPlane;
    m_Far = farPlane;
    m_ProjectionMatrix = Matrix4x4::Perspective(fovDegrees, aspect, nearPlane, farPlane);
}

void Camera::SetOrthographic(float size, float aspect, float nearPlane, float farPlane)
{
    m_Orthographic = true;
    m_OrthoSize = size;
    m_Aspect = aspect;
    m_Near = nearPlane;
    m_Far = farPlane;

    float half = size * 0.5f;
    m_ProjectionMatrix = Matrix4x4::Ortho(-half * aspect, half * aspect, -half, half, nearPlane, farPlane);
}

void Camera::RecalculateViewMatrix()
{
    Transform& transform = GetOwner()->GetTransform();
    m_ViewMatrix = Matrix4x4::LookAt(
        transform.GetWorldPosition(),
        transform.GetWorldPosition() + transform.GetForward(),
        transform.GetUp()
    );
}

} // namespace Leir
