# Math Library Migration

Wrap GLM types into custom LeirEngine math types (`Vector2`, `Vector3`, `Quaternion`, `Matrix4x4`, etc.) with zero-cost abstractions and implicit conversion to GLM for incremental migration.

## Priority Order

### Phase 1 — Foundation (no deps)

- [x] `TODO_MATH.md` created
- [x] `Mathf.h` — Lerp, Clamp, Approximately, SmoothStep, Deg2Rad, Rad2Deg, PI, Epsilon, Sign, Abs, Min, Max, Floor, Ceil, Round, PingPong, Repeat, MoveTowards, SmoothDamp, DeltaAngle, InverseLerp, Clamp01, etc.
- [x] `Vector2.h` — wrapper over `glm::vec2` (Dot, Cross2D, Distance, Angle, Lerp, Perpendicular, ClampMagnitude, Zero, One)
- [x] `Vector3.h` — wrapper over `glm::vec3` (Dot, Cross, Distance, Angle, Slerp, OrthoNormalize, Project, Reflect, Zero, One, Forward, Up, Right, Back, Down, Left)
- [x] `Vector2i.h` — wrapper over `glm::ivec2` (Min, Max, Clamp, Zero, One)

### Phase 2 — Rotation & Matrices

- [x] `Vector3i.h` — wrapper over `glm::ivec3`
- [x] `Vector4.h` — wrapper over `glm::vec4` (+ conversions to/from Vector3)
- [x] `Quaternion.h` — wrapper over `glm::quat` (Euler, LookRotation, FromToRotation, Slerp, AngleAxis, Inverse, RotateTowards, Identity)
- [x] `Matrix4x4.h` — wrapper over `glm::mat4` (TRS, Perspective, Ortho, LookAt, Inverse, Transpose, MultiplyPoint3x4)
- [x] `Matrix3x3.h` — wrapper over `glm::mat3` (normal transforms, pure 2D)

### Phase 3 — Primitives & Geometry

- [x] `Ray.h` — Vector3 origin + Vector3 direction (GetPoint)
- [x] `Plane.h` — Vector3 normal + float distance (Raycast, GetDistanceToPoint, ClosestPointOnPlane, GetSide)
- [x] `Bounds.h` — AABB (Contains, Intersects, Encapsulate, Expand, center, size, extents, min, max)
- [x] `Rect.h` — float x, y, width, height (Contains, Overlaps)
- [x] `RectInt.h` — int x, y, width, height

### Phase 4 — Utilities & Color

- [x] `Color.h` — wrapper over `glm::vec4` (0-1 range, HSVToRGB, RGBToHSV, Lerp)
- [x] `Color32.h` — wrapper over `glm::u8vec4` (0-255 range)
- [x] `Random.h` — deterministic random with seed (mt19937-based, Range, Value)
- [x] `Frustum.h` — Plane[6] + Intersects(Bounds) for camera culling

### Phase 5 — Migration

Cada wrapper tiene `operator glm::*()` implícito, así que la migración es gradual sin romper nada.

**Estrategia por capa:**
1. ✅ Core types (Transform, Object3D) — declaran `Vector3`, `Quaternion`
2. Components (Camera, Light, MeshRenderer) — reemplazan `glm::vec3/mat4`
3. Rendering (RenderPipeline, Shader, Material, Mesh) — matrices a `Matrix4x4`
4. Scene — `Ray`, `Bounds` para raycasting y culling
5. Input — `glm::vec2` → `Vector2` en posición/delta (headers públicos)
6. UI — `glm::vec2/4` → `Vector2/4`, `Rect2D` usa `Rect` internamente
7. Editor — `main.cpp`, `EditorCamera.cpp`, `UITestPanel/CameraTestPanel`

**Lo que NO cambia:**
- GLM sigue como dependencia directa (los wrappers lo envuelven)
- `glm::radians/degrees`, `glm::sin/cos`, etc. siguen disponibles
- `glm::value_ptr` / `glm::make_mat4` para interop con Vulkan se quedan
- `GLM_ENABLE_EXPERIMENTAL` donde sea necesario

**Detalle por paso:**

### Paso 1 — Core/Transform.h + Transform.cpp

| Archivo | Cambio |
|---|---|
| `Transform.h` | `glm::vec3 m_LocalPosition/Scale` → `Vector3`; `glm::quat m_LocalRotation` → `Quaternion`; getters/setters devuelven `Vector3`/`Quaternion` |
| `Transform.cpp` | Reemplazar `glm::normalize`, `glm::dot`, `glm::cross`, `glm::lerp` por `Vector3::Normalized`, `Vector3::Dot`, `Vector3::Cross`, `Mathf::Lerp` |

- [x] **Paso 1** — Core/Transform.h + Transform.cpp

### Paso 2 — Core/CoreObject.h

- [x] Reemplazar `glm::vec3` → `Vector3` en campos de CoreObject (si los hay) — 0 glm refs, ya usa Transform

### Paso 3 — Components/

- [x] **Paso 3a** — Camera.h: `glm::mat4` → `Matrix4x4`
- [x] **Paso 3b** — Camera.cpp: `glm::perspective/ortho/lookAt` → `Matrix4x4::Perspective/Ortho/LookAt`
- [x] **Paso 3c** — Light.h: `glm::vec3 color` → `Vector3`
- [x] **Paso 3d** — Light.cpp: `GetDirection()` → `Vector3`
- [x] **Paso 3e** — MeshRenderer.h: no GLM refs

### Paso 4 — Objects/

- [x] **Paso 4a** — Object3D.h: `glm::vec3` → `Vector3` (bounds)
- [x] **Paso 4b** — Object3D.cpp: implementación

### Paso 5 — Rendering/

- [x] **Paso 5a** — RenderPipeline.h/.cpp: `glm::mat4/vec3/vec4` → `Matrix4x4/Vector3/Vector4`
- [x] **Paso 5b** — RenderTexture.h/.cpp: no `glm::` refs
- [x] **Paso 5c** — Shader.h/.cpp: no `glm::` refs
- [x] **Paso 5d** — Mesh.h/.cpp: `Vertex` struct + primitives `glm::vec3/2` → `Vector3/2`
- [x] **Extra** — SpriteRenderer.h: `glm::vec4/2` → `Vector4/2`
- [x] **Extra** — Image.h/.cpp: `glm::vec4` → `Vector4`, `glm::clamp` → `Mathf::Clamp`
- [x] **Extra** — Material.h/.cpp: `glm::vec4/3` → `Vector4/3`
- [x] **Extra** — SpriteSheet.h/.cpp: `glm::vec4` → `Vector4`

### Paso 6 — Input/

- [ ] **Paso 6a** — Mouse.h: `glm::vec2` → `Vector2`
- [ ] **Paso 6b** — Mouse.cpp: implementación
- [ ] **Paso 6c** — Touch.h: `glm::vec2` → `Vector2`
- [ ] **Paso 6d** — Pointer.h: `glm::vec2` → `Vector2`
- [ ] **Paso 6e** — InputEvent.h: `glm::vec2 pos/delta` → `Vector2`

### Paso 7 — UI/

- [ ] **Paso 7a** — Rect2D.h: usar `Rect` internamente o reemplazar `glm::vec2/4`
- [ ] **Paso 7b** — UIElement.h: `glm::vec2/4` → `Vector2/4`
- [ ] **Paso 7c** — UICanvas.h: `glm::vec2` → `Vector2`
- [ ] **Paso 7d** — UILabel.h: `glm::vec4` → `Vector4`
- [ ] **Paso 7e** — UIRenderer.h: `glm::vec2` → `Vector2`, `glm::vec4` → `Vector4`
- [ ] **Paso 7f** — Fuentes de UI (UIElement.cpp, UIRenderer.cpp, etc.)

### Paso 8 — Editor

- [ ] **Paso 8a** — EditorCamera.h/.cpp: `glm::vec3` → `Vector3`, `glm::quat` → `Quaternion`
- [ ] **Paso 8b** — main.cpp: migrar todo
- [ ] **Paso 8c** — UITestPanel.h/.cpp: migrar
- [ ] **Paso 8d** — CameraTestPanel.h/.cpp: migrar

### Paso 9 — Limpieza

- [ ] Remover `#include <glm/...>` directos de headers públicos (los wrappers lo incluyen internamente)
- [ ] Quitar `GLM_ENABLE_EXPERIMENTAL` de archivos que ya usan wrappers
- [ ] Verificar que no queden referencias directas a `glm::` en public headers
