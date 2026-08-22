# Transform Gizmo System

Sistema de gizmos de transformación 3D estilo Unity (traslación, rotación, escalado)
para el editor. Estado: **EN DESARROLLO** (fase 5 en curso).

## Concepto

- Gizmos sobre el objeto **seleccionado** (click-pick por raycast en el viewport).
- Herramientas activadas desde la **toolbar** superior (no dockerizable) y por
  atajos de teclado **W/E/R**.
- Modo **Global / Local** con toggle en la toolbar (grisado en Scale).
- Render: líneas de ancho constante en píxeles (GizmoRenderer) + geometría
  rellena (conos, cubos, cuadrados translúcidos) con pipeline nuevo
  `GizmoSolid.vert/frag.slang`.
- **Sin gimbal lock**: la rotación se acumula SIEMPRE como quaternion
  (`AngleAxis(delta, eje) * rot`). Nunca se descompone a Euler en el gizmo.

## Arquitectura

```
EditorApp (main.cpp)
├── ToolbarPanel (nuevo, editor/src/UI/)      → botones W/E/R + Global/Local
├── TransformGizmo (nuevo, editor/src/Gizmos/) → estado, geometría, picking, drag
│     └── usa GizmoRenderer como salida de primitivas (líneas + sólidos)
├── m_SelectedObject (Object3D*)              → selección mínima por raycast
└── DockManager (baja su top para dejar lugar a la toolbar)
```

## Colores de ejes (idénticos en los 3 gizmos)

| Eje | Color |
|---|---|
| X | Rojo |
| Y | Verde |
| Z | Azul |
| Hover | lerp hacia blanco |

## Checkboxes

### Fase 0 — Documento de registro
- [x] `TRANSFORM_GIZMOS_SYSTEM.md` creado con plan + checkboxes.

### Fase 1 — Infraestructura de render (sólidos)
- [x] `engine/shaders/GizmoSolid.vert.slang` + `.frag.slang` (triángulos rellenos, color por vértice, UBO viewProjection, blend alpha, depthTest on / depthWrite off).
- [x] `GizmoRenderer` extendido: 2º pipeline + 2º vertex buffer (sólidos).
- [x] `GizmoRenderer::DrawTriangle` / `DrawCubeFilled` / `DrawCone` / `DrawQuadFilled` / `DrawArc`.
- [x] Shaders registrados en `ShaderExporter::ShaderFiles()` + `ShaderHotReloader` + `engine/CMakeLists.txt`.

### Fase 2 — Selección mínima
- [x] `m_SelectedObject` en `EditorApp` (default Cube).
- [x] Raycast mouse→rayo con `viewProjection.Inverse()`; `ray vs AABB` → pick más cercano.
- [x] Click izquierdo en viewport selecciona; click en vacío deselecciona.
- [x] Box violeta wireframe (DrawBox) alrededor del seleccionado.

### Fase 3 — TransformGizmo (núcleo)
- [x] Estado: `Tool {Translate, Rotate, Scale}`, `Space {Global, Local}`, hover, drag.
- [x] `m_GizmoRotation` (quat): Global = acumula deltas (reset al reseleccionar); Local = rotación del objeto.
- [x] **Translate**: 3 flechas (línea + cono) + 3 cuadrados translúcidos de área.
- [x] Drag de eje (closest-point entre rayo y línea del eje).
- [x] Drag de plano (intersección rayo↔plano, bloquea eje perpendicular).
- [x] **Rotate**: 3 aros, SOLO el medio-arco que mira a la cámara (estilo Unity/Godot, la parte de atrás no se dibuja).
- [x] Drag de rotación: `atan2` en el plano del aro → `AngleAxis(delta, normal) * rot` (acumulado). Sin Euler → sin gimbal lock.
- [x] **Scale**: 3 flechas con cubos (siempre local) + cubo central gris (uniforme).
- [x] Hover más claro sobre el handle.

### Fase 4 — Toolbar
- [x] `ToolbarPanel` (hermano del DockManager, anchor top, no dockerizable, full width).
- [x] Botones W/E/R radiogrupo (el activo se grisa, los inactivos habilitados).
- [x] Toggle Global/Local (grisado/deshabilitado en Scale).
- [x] Shortcuts W/E/R (`Keyboard::WasPressed`) con guardas: no al volar cámara (right/middle down), no durante drag de gizmo, no con foco en UITextInput.
- [x] DockManager baja su top: `offset {0, 30, 0, -30}`.

### Fase 5 — Integración + verificación
- [x] `main.cpp`: crear toolbar + gizmo, `TransformGizmo::Update` en OnUpdate, `Draw` en OnRender tras el grid, click-pick.
- [x] Build local (MSVC) + SlangExportTest 12/12 (GizmoSolid incluido) + ctest 2/2.
- [x] Prueba manual: build + launch limpio (D3D12), sin overflow (fixed: BeginFrame limpia m_SolidVerts), sin VUIDs/stderr. Falta verificación visual del usuario.

## Decisiones de diseño

1. **Rotación sin gimbal lock**: el drag de rotación acumula como quaternion
   (`AngleAxis(delta, normalAro) * rotActual`). Los paneles de inspección usan
   Euler solo para display; el gizmo nunca toca Euler.
2. **Semi circunferencias frontales**: en cada aro se dibuja solo la mitad cuyo
   ángulo central apunta hacia la cámara (`dot(P−centro, camPos−centro) > 0`),
   igual que Unity/Godot — la parte de atrás no se ve.
3. **Scale siempre local**: el toggle Global/Local se grisa en Scale (un solo modo).
4. **Toolbar no dockerizable**: es hermano del DockManager en el Canvas, no un
   dock panel; anclado arriba con ancho completo.
5. **Click-pick mínimo en esta fase**: raycast contra AABB; la hierarchy panel
   (fase siguiente) solo reflejará el mismo estado de selección.
6. **Outline provisional**: box violeta wireframe con DrawBox. El outline shader
   de silueta real queda para la fase hierarchy.