#include "LeirEngine/Components/Light.h"
#include "LeirEngine/Core/CoreObject.h"
#include "LeirEngine/Core/Transform.h"

namespace Leir {

glm::vec3 Light::GetDirection() const
{
    return GetOwner()->GetTransform().GetForward();
}

} // namespace Leir
