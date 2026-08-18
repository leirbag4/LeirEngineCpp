#pragma once
#include <LeirEngine/Math/Vector3.h>
#include <LeirEngine/Math/Quaternion.h>

class EditorCamera {
public:
    EditorCamera();

    void Update(float deltaTime);

    Leir::Vector3 GetPosition() const { return m_Position; }
    float GetYaw() const { return m_Yaw; }
    float GetPitch() const { return m_Pitch; }
    Leir::Vector3 GetForward() const;
    Leir::Vector3 GetRight() const;
    Leir::Quaternion GetRotation() const;

    void SetPosition(const Leir::Vector3& pos) { m_Position = pos; }
    void SetYaw(float y) { m_Yaw = y; }
    void SetPitch(float p) { m_Pitch = Leir::Mathf::Clamp(p, -89.0f, 89.0f); }

private:
    // TEMP-DEBUG (PHASE-1 TEST): 3/4 view, off the X+Z diagonal, so the three
    // test gizmo lines (red X axis, blue Z axis, white diagonal) are all
    // clearly visible and separated.
    Leir::Vector3 m_Position = {4.0f, 2.0f, 5.0f};
    float m_Yaw = 35.0f;
    float m_Pitch = -25.0f;
};
