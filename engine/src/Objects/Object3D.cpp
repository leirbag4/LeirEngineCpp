#include "LeirEngine/Objects/Object3D.h"

namespace Leir {

Object3D::Object3D(const std::string& name)
    : CoreObject(name)
{
}

void Object3D::SetBounds(const Vector3& min, const Vector3& max)
{
    m_BoundsMin = min;
    m_BoundsMax = max;
}

} // namespace Leir
