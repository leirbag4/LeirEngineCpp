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

    // Inverse of GetRotation() (which is always Euler(pitch, yaw, 0), roll=0).
    // Do NOT use Quaternion::ToEuler for this: glm::eulerAngles returns an
    // equivalent ALIAS (yaw -> 180-yaw, roll -> ±180) once |yaw|>90, which
    // corrupts the accumulated yaw/pitch on the scene -> editor sync.
    void SetFromRotation(const Leir::Quaternion& rot);

private:
    Leir::Vector3 m_Position = {0.0f, 2.0f, 4.0f};
    float m_Yaw = 0.0f;
    float m_Pitch = -20.0f;
};
