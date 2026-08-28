#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Matrix4x4.h"
#include "LeirEngine/Math/Quaternion.h"
#include "LeirEngine/Math/Vector3.h"
#include "LeirEngine/ECS/Entity.h"

#include <cstdint>
#include <vector>

namespace Leir {
namespace ECS {

class World;
class HierarchyTree;

// POD transform components (data-oriented, no logic). LocalTransform is the
// authored TRS in parent space; WorldTransform is the computed world matrix +
// world pos/rot/LOSSY scale (column lengths), produced by TransformSystem.
struct LEIR_API LocalTransform {
    Vector3 position = Vector3::Zero();
    Quaternion rotation = Quaternion::Identity();
    Vector3 scale = Vector3::One();
};

struct LEIR_API WorldTransform {
    Matrix4x4 worldMatrix = Matrix4x4::Identity();
    Vector3 worldPosition = Vector3::Zero();
    Quaternion worldRotation = Quaternion::Identity();
    Vector3 worldScale = Vector3::One();
};

// Computes WorldTransform from LocalTransform + HierarchyTree, top-down, via a
// dirty frontier (only mutated subtrees are recomputed; parents are ensured
// before children). Reparent honors worldPositionStays using the EXACT
// lossy-scale preserve (divide by the column lengths of parentWorld · localRot)
// plus the epsilon guard for zero-scaled axes — same math as Transform.cpp.
class LEIR_API TransformSystem {
public:
    TransformSystem(World* world, HierarchyTree* tree)
        : m_World(world)
        , m_Tree(tree)
    {
    }

    void SetLocal(Entity e, const LocalTransform& lt);
    LocalTransform* GetLocal(Entity e);
    // Ensures the entity's WorldTransform is up to date and returns it.
    WorldTransform* GetWorld(Entity e);
    void SetParent(Entity e, Entity parent, bool worldPositionStays = true);

    // World-space setters (Etapa A: the Transform facade delegates here): recompute
    // the local so the requested world is met, exact lossy-preserve included.
    void SetWorldPosition(Entity e, const Vector3& position);
    void SetWorldRotation(Entity e, const Quaternion& rotation);
    void SetWorldScale(Entity e, const Vector3& scale);

    void Update();

private:
    void MarkSubtreeDirty(uint32_t ei);
    void EnsureClean(uint32_t ei);
    bool IsDirty(uint32_t ei) const { return ei < m_Dirty.size() && m_Dirty[ei] != 0; }
    void SetDirty(uint32_t ei, bool dirty);

    World* m_World;
    HierarchyTree* m_Tree;
    std::vector<uint8_t> m_Dirty;
    std::vector<uint32_t> m_DirtyList;
};

} // namespace ECS
} // namespace Leir