#include "LeirEngine/Core/Component.h"
#include "LeirEngine/Core/CoreObject.h"
#include "LeirEngine/Scene/Scene.h"

namespace Leir {

Scene* Component::GetScene() const
{
    return m_Owner ? m_Owner->GetScene() : nullptr;
}

} // namespace Leir
