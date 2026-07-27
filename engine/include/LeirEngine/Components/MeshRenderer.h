#pragma once

#include "LeirEngine/Core/Component.h"
#include <memory>

namespace Leir {

class Mesh;
class Material;

class LEIR_API MeshRenderer : public Component {
public:
    MeshRenderer() = default;

    void SetMesh(std::shared_ptr<Mesh> mesh) { m_Mesh = mesh; }
    void SetMaterial(std::shared_ptr<Material> material) { m_Material = material; }

    std::shared_ptr<Mesh> GetMesh() const { return m_Mesh; }
    std::shared_ptr<Material> GetMaterial() const { return m_Material; }

    void OnUpdate(float deltaTime) override;

private:
    std::shared_ptr<Mesh> m_Mesh;
    std::shared_ptr<Material> m_Material;
};

} // namespace Leir
