#pragma once

#include "LeirEngine/Core/Component.h"
#include "LeirEngine/Core/ComponentTraits.h"
#include "LeirEngine/Math/Vector3.h"

namespace Leir {

enum class LightType {
    Directional,
    Point,
    Spot
};

class LEIR_API Light : public Component {
public:
    Light() = default;

    void SetType(LightType type) { m_Type = type; }
    LightType GetType() const { return m_Type; }

    void SetColor(const Vector3& color) { m_Color = color; }
    const Vector3& GetColor() const { return m_Color; }

    void SetIntensity(float intensity) { m_Intensity = intensity; }
    float GetIntensity() const { return m_Intensity; }

    void SetRange(float range) { m_Range = range; }
    float GetRange() const { return m_Range; }

    void SetSpotAngle(float innerDeg, float outerDeg) {
        m_SpotInnerAngle = innerDeg;
        m_SpotOuterAngle = outerDeg;
    }

    Vector3 GetDirection() const;

private:
    LightType m_Type = LightType::Directional;
    Vector3 m_Color{1.0f, 1.0f, 1.0f};
    float m_Intensity = 1.0f;
    float m_Range = 10.0f;
    float m_SpotInnerAngle = 30.0f;
    float m_SpotOuterAngle = 45.0f;
};

} // namespace Leir

template<>
struct Leir::IsDataComponent<Leir::Light> : std::true_type {
};
