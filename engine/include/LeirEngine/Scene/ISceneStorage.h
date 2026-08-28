#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Scene/SceneGroups.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Leir {

class CoreObject;
class Object3D;
class Object2D;

// Storage seam for a Scene's world data (Fase 0, TODO_HYBRID_ECS.md). The
// concrete Scene implements this today over the custom ECS. Renderers, picking
// and every other consumer keep working with the friendly CoreObject API.
class LEIR_API ISceneStorage {
public:
    virtual ~ISceneStorage() = default;

    // Structural operations (authoritative scene order for roots).
    virtual Object3D* CreateObject3D(const std::string& name) = 0;
    virtual Object2D* CreateObject2D(const std::string& name) = 0;
    virtual void DestroyObject(CoreObject* object) = 0;
    virtual void MoveObject(CoreObject* object, size_t index) = 0;

    // Queries.
    virtual CoreObject* FindObjectByUUID(uint64_t uuid) const = 0;
    virtual CoreObject* FindObjectByName(const std::string& name) const = 0;
    virtual const std::vector<std::unique_ptr<CoreObject>>& GetObjects() const = 0;

    // Data-oriented query caches, rebuilt lazily on structural change. They cover
    // the WHOLE hierarchy and include inactive objects too (callers filter via
    // IsActive). Renderers iterate these instead of scanning GetObjects() +
    // GetComponent<T>() every frame.
    virtual const std::vector<CoreObject*>& GetRenderables() = 0;
    virtual const std::vector<CoreObject*>& GetCameras() = 0;
    virtual const std::vector<CoreObject*>& GetLights() = 0;

    // Journal-synced ECS query groups (resto c): the renderer's primary path.
    // ForEach gives (component&, Active&, WorldTransform&, Entity) — contiguous
    // member iteration with no per-frame object/component lookups.
    virtual SceneGroups::Renderables& GetRenderGroup() = 0;
    virtual SceneGroups::Sprites& GetSpriteGroup() = 0;
    virtual SceneGroups::Cameras& GetCameraGroup() = 0;
    virtual SceneGroups::Lights& GetLightGroup() = 0;

    // Called by CoreObject when a component is added/removed or the object is
    // reparented: invalidates the query caches.
    virtual void MarkCachesDirty() = 0;
};

} // namespace Leir