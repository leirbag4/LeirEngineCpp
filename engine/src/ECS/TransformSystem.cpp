#include "LeirEngine/ECS/TransformSystem.h"
#include "LeirEngine/ECS/HierarchyTree.h"
#include "LeirEngine/ECS/World.h"
#include "LeirEngine/Math/Mathf.h"

namespace Leir {
namespace ECS {

static Vector3 ComputeColumnLengths(const Matrix4x4& m)
{
    return Vector3(
        Mathf::Sqrt(m(0,0) * m(0,0) + m(1,0) * m(1,0) + m(2,0) * m(2,0)),
        Mathf::Sqrt(m(0,1) * m(0,1) + m(1,1) * m(1,1) + m(2,1) * m(2,1)),
        Mathf::Sqrt(m(0,2) * m(0,2) + m(1,2) * m(1,2) + m(2,2) * m(2,2)));
}

static void ComputeWorld(const LocalTransform& lt, const WorldTransform* parentWT, WorldTransform& out)
{
    const Matrix4x4 local = Matrix4x4::TRS(lt.position, lt.rotation, lt.scale);
    if (!parentWT) {
        out.worldMatrix = local;
        out.worldPosition = lt.position;
        out.worldRotation = lt.rotation;
    } else {
        out.worldMatrix = parentWT->worldMatrix * local;
        out.worldRotation = parentWT->worldRotation * lt.rotation;
        out.worldPosition = parentWT->worldMatrix.MultiplyPoint3x4(lt.position);
    }
    out.worldScale = ComputeColumnLengths(out.worldMatrix);
}

void TransformSystem::SetLocal(Entity e, const LocalTransform& lt)
{
    if (!e)
        return;
    m_World->Add<LocalTransform>(e) = lt;
    MarkSubtreeDirty(e.index);
}

LocalTransform* TransformSystem::GetLocal(Entity e)
{
    return e ? m_World->Get<LocalTransform>(e) : nullptr;
}

namespace {
// Returns the entity's local (default identity when absent) for the world setters.
LocalTransform LocalOrDefault(World* world, Entity e)
{
    LocalTransform* lt = world->Get<LocalTransform>(e);
    return lt ? *lt : LocalTransform{};
}
} // namespace

void TransformSystem::SetWorldPosition(Entity e, const Vector3& position)
{
    if (!e)
        return;
    LocalTransform lt = LocalOrDefault(m_World, e);
    const uint32_t parent = m_Tree->GetParent(e.index);
    if (parent == kNullIndex) {
        lt.position = position;
        SetLocal(e, lt);
        return;
    }
    EnsureClean(parent);
    const WorldTransform* pwt = m_World->Get<WorldTransform>(Entity{parent, m_World->GenerationOf(parent)});
    if (pwt) {
        const Matrix4x4 inv = pwt->worldMatrix.Inverse();
        lt.position = inv.IsFinite() ? inv.MultiplyPoint3x4(position) : position;
    }
    SetLocal(e, lt);
}

void TransformSystem::SetWorldRotation(Entity e, const Quaternion& rotation)
{
    if (!e)
        return;
    LocalTransform lt = LocalOrDefault(m_World, e);
    const uint32_t parent = m_Tree->GetParent(e.index);
    if (parent == kNullIndex) {
        lt.rotation = rotation;
        SetLocal(e, lt);
        return;
    }
    EnsureClean(parent);
    const WorldTransform* pwt = m_World->Get<WorldTransform>(Entity{parent, m_World->GenerationOf(parent)});
    if (pwt)
        lt.rotation = pwt->worldRotation.Inverse() * rotation;
    SetLocal(e, lt);
}

void TransformSystem::SetWorldScale(Entity e, const Vector3& scale)
{
    if (!e)
        return;
    LocalTransform lt = LocalOrDefault(m_World, e);
    const uint32_t parent = m_Tree->GetParent(e.index);
    if (parent == kNullIndex) {
        lt.scale = scale;
        SetLocal(e, lt);
        return;
    }
    EnsureClean(parent);
    const WorldTransform* pwt = m_World->Get<WorldTransform>(Entity{parent, m_World->GenerationOf(parent)});
    if (pwt) {
        // Exact lossy-preserve: divide by |(parentWorld · localRot) column| so the
        // world LOSSY scale is preserved under a rotated + non-uniformly-scaled
        // parent. Epsilon guard for zero-scaled axes (0/0 -> 0, no NaN).
        const Matrix4x4 localRotM = Matrix4x4::TRS(Vector3::Zero(), lt.rotation, Vector3::One());
        const Matrix4x4 combined = pwt->worldMatrix * localRotM;
        const Vector3 colLen = ComputeColumnLengths(combined);
        constexpr float kEps = 1e-8f;
        lt.scale = Vector3(
            scale.x / (colLen.x > kEps ? colLen.x : 1.0f),
            scale.y / (colLen.y > kEps ? colLen.y : 1.0f),
            scale.z / (colLen.z > kEps ? colLen.z : 1.0f));
    }
    SetLocal(e, lt);
}

WorldTransform* TransformSystem::GetWorld(Entity e)
{
    if (!e)
        return nullptr;
    EnsureClean(e.index);
    return m_World->Get<WorldTransform>(e);
}

void TransformSystem::SetParent(Entity e, Entity parent, bool worldPositionStays)
{
    if (!e)
        return;
    if (!m_World->IsAlive(e))
        return;

    EnsureClean(e.index);
    const WorldTransform* wt = m_World->Get<WorldTransform>(e);
    const Vector3 wPos = wt ? wt->worldPosition : Vector3::Zero();
    const Quaternion wRot = wt ? wt->worldRotation : Quaternion::Identity();
    const Vector3 wScale = wt ? wt->worldScale : Vector3::One();

    m_Tree->SetParent(e.index, parent ? parent.index : kNullIndex);

    if (!worldPositionStays) {
        MarkSubtreeDirty(e.index);
        return;
    }

    // worldPositionStays: recompute the LOCAL transform so the WORLD stays
    // exactly as captured.
    if (!parent) {
        SetLocal(e, LocalTransform{wPos, wRot, wScale});
        return;
    }
    EnsureClean(parent.index);
    const WorldTransform* parentWT = m_World->Get<WorldTransform>(parent);
    if (!parentWT) {
        // Parent has no world transform (no LocalTransform): identity parent.
        SetLocal(e, LocalTransform{wPos, wRot, wScale});
        return;
    }

    Vector3 localPos;
    Matrix4x4 parentWorldInv = parentWT->worldMatrix.Inverse();
    if (parentWorldInv.IsFinite()) {
        localPos = parentWorldInv.MultiplyPoint3x4(wPos);
    } else {
        // Singular parent matrix (zero-scaled axis): its inverse is non-finite
        // and any local is arbitrary. Keep the captured world position as local
        // rather than poisoning the chain with NaN.
        localPos = wPos;
    }
    Quaternion localRot = parentWT->worldRotation.Inverse() * wRot;

    // Lossy-preserve (exact, better than Unity): the child's world LOSSY scale
    // is localScale · |(parentWorld · localRot) column|; dividing by those
    // column lengths preserves it under a rotated + non-uniformly-scaled parent.
    // Epsilon guard: a zero-scaled parent axis collapses the column -> 0/0 NaN;
    // fall back to 1.0 so the result stays 0 (degenerate world anyway).
    const Matrix4x4 localRotM = Matrix4x4::TRS(Vector3::Zero(), localRot, Vector3::One());
    const Matrix4x4 combined = parentWT->worldMatrix * localRotM;
    const Vector3 colLen = ComputeColumnLengths(combined);
    constexpr float kEps = 1e-8f;
    Vector3 localScale(
        wScale.x / (colLen.x > kEps ? colLen.x : 1.0f),
        wScale.y / (colLen.y > kEps ? colLen.y : 1.0f),
        wScale.z / (colLen.z > kEps ? colLen.z : 1.0f));

    SetLocal(e, LocalTransform{localPos, localRot, localScale});
}

void TransformSystem::Update()
{
    for (uint32_t ei : m_DirtyList)
        EnsureClean(ei);
    m_DirtyList.clear();
}

void TransformSystem::MarkSubtreeDirty(uint32_t ei)
{
    SetDirty(ei, true);
    for (uint32_t child = m_Tree->GetFirstChild(ei); child != kNullIndex; child = m_Tree->GetNextSibling(child))
        MarkSubtreeDirty(child);
}

void TransformSystem::EnsureClean(uint32_t ei)
{
    if (!IsDirty(ei))
        return;

    Entity e{ei, m_World->GenerationOf(ei)};
    LocalTransform* lt = m_World->Get<LocalTransform>(e);
    if (!lt) {
        m_World->Remove<WorldTransform>(e);
        SetDirty(ei, false);
        return;
    }

    const uint32_t parent = m_Tree->GetParent(ei);
    if (parent != kNullIndex)
        EnsureClean(parent);

    // Copy the parent's world BY VALUE before Add<WorldTransform>: adding the
    // child's component can reallocate the WorldTransform pool, which would
    // invalidate a pointer taken into it (reading freed memory is UB and
    // manifested as NaN on some compilers/allocators — AppleClang vs MSVC).
    WorldTransform parentData;
    const WorldTransform* parentWT = parent != kNullIndex
        ? m_World->Get<WorldTransform>(Entity{parent, m_World->GenerationOf(parent)})
        : nullptr;
    if (parentWT)
        parentData = *parentWT;

    WorldTransform& wt = m_World->Add<WorldTransform>(e);
    ComputeWorld(*lt, parentWT ? &parentData : nullptr, wt);
    SetDirty(ei, false);
}

void TransformSystem::SetDirty(uint32_t ei, bool dirty)
{
    if (m_Dirty.size() <= ei)
        m_Dirty.resize((size_t)ei + 1, 0);
    const bool was = m_Dirty[ei] != 0;
    m_Dirty[ei] = dirty ? 1 : 0;
    if (dirty && !was)
        m_DirtyList.push_back(ei);
}

} // namespace ECS
} // namespace Leir