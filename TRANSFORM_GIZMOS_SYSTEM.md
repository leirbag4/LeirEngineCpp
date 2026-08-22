# Transform Gizmo System

Sistema de gizmos de transformación 3D estilo Unity (traslación, rotación, escalado)
para el editor. Estado: **COMPLETO** (verificado por el usuario, ctest 2/2, CI verde).

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

### Fase 0b — Picking de planos por RAYCAST 3D (fix del hover inestable al reorientar)
- [x] **Bug**: al reorientarse los planos hacia la cámara (estilo Unity), el hover se volvía inestable — el verde no se agarraba y se seleccionaba el rojo / la flecha / nada. **Causa raíz**: el `Pick` usaba **point-in-polygon proyectado a pantalla** (`PointInQuadPx`) + desempate con `RayPlane` contra el **plano infinito**. Al reorientarse, los 3 quads (que comparten la esquina en `g.center`) se **superponen en pantalla** y el winding proyectado se invierte → el test fallaba o elegía el quad equivocado; cuando el mouse caía en un hueco, la flecha robaba el hover.
- [x] **Fix**: nuevo `TransformGizmo::RayQuadHit` — intersecta el rayo del mouse con el **quad finito 3D real** (paralelogramo `p0 + u*(p1-p0) + v*(p3-p0)`, `u,v ∈ [0,1]`), no con el plano infinito. El `Pick` de translate usa ese raycast contra la **misma geometría exacta del Draw** (esquina compartida en `g.center`, `su/sv` hacia cámara — los planos NO se separan). Gana el quad de menor profundidad (más cercano a la cámara). Eliminado `PointInQuadPx` y `kPlanePickPx` (sin uso).
- [x] **Por qué funciona**: los 3 quads 3D viven en planos distintos (XZ/YZ/XY) y solo se tocan en las aristas — no se superponen en 3D. El raycast al quad real es estable en cualquier orientación; la profundidad del hit decide cuál está delante en el punto exacto del mouse.
- [x] Verificado por el usuario: mismo setup (cámara `pos 1.56,1.216,-1.08` rot `165.59,42.8,180`), el plano verde se agarra estable con el mouse encima (`hover=PlaneY` sostenido) y el click lo arrastra. ctest 2/2.

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
- [x] **Fix layout drift al mover el dock bajo la toolbar** (2026-08-22): `ComputeFreeLayout` hace `child->m_Rect.offset.top += m_ComputedRect.y` — MUTACIÓN PERMANENTE del offset del hijo. Antes el DockManager tenía Y computado = 0 (el `+=0` era no-op); al bajarlo a `y=30` sumaba +30 al offset del nodo root del dock **cada frame** → todo se deslizaba hacia abajo hasta desaparecer. Fix: `DockManager::ComputeLayout` override que posiciona sus hijos (root + DockDropOverlay) con rects ABSOLUTAS (set, no `+=`) cada frame; `DockDropOverlay::ComputeLayout` override para no sumar su posición a ghost/zone (que ya son coords absolutas de pantalla). Verificado: rect del dock estable `(0,30,W,H-60)` en frames 0/120/240/360/480.

### Bugs de interacción corregidos (2026-08-22, verificados por el usuario en el editor)
- [x] **Drag de ejes (translate y scale) no movía nada**: el plano de drag usaba normal `cross(axisDir, viewDir)`, que contiene el eje Y la cámara → la cámara queda DENTRO del plano → la intersección rayo-plano siempre cae en el origen del rayo (t≈0) → `hit` constante → delta=0. Fix: **closest-point entre el rayo del mouse y la línea del eje** (`ClosestPointOnAxis`, fórmula estándar rayo-línea), robusto a cualquier ángulo de cámara. Ahora las 3 flechas trasladan/escalan.
- [x] **Drift decimal en los planos de área** (el verde sumaba +0.000002..+0.000136 a Y): el plano se anclaba a `g.center` (el centro **en movimiento**) → el rayo-plano se recomputaba contra un plano que se movía cada frame → error de float acumulado. Fix: anclar el plano a `m_Drag.startPos` (posición inicial, fija) + **proyectar el delta quitando el componente normal** (`delta - normal * dot(delta, normal)`). Ahora el eje bloqueado queda exactamente en su valor inicial.
- [x] **Rotación usa startPos como ancla del aro** (mismo patrón anti-drift).
- [x] **Cubos de scale no rotaban con el objeto**: se dibujaban axis-aligned (`DrawCubeFilled`). Fix: nuevo `GizmoRenderer::DrawCubeFilledOriented(center, size, rotation, color)` que rota los 8 corners; los cubos de punta y el central se orientan con la **world rotation del objeto** (scale siempre local).
- [x] **El cubo central de scale perdía el hover contra las flechas** (siempre se resaltaba una flecha): en `Pick` las flechas se testean primero y el cubo central después; como los ejes nacen en el centro, el mouse sobre el cubo central también quedaba dentro del pick de la flecha. Fix: en scale el cubo central se testea PRIMERO y retorna inmediato (prioridad máxima).
- [x] **Gizmos tapados por la geometría del objeto** (2026-08-22): ambas pipelines del `GizmoRenderer` (línea + sólida) tenían `depthTestEnable = true` → las flechas/conos/cubos quedaban ocultos detrás del objeto seleccionado. Fix: `depthTestEnable = false` en las dos pipelines (los gizmos se dibujan POR ARRIBA de la escena, como la capa de gizmos de Unity). El grid conserva su depth test (los objetos lo tapan — comportamiento deseado).
- [x] **Cuadraditos de plano ocultos entre sí al girar la cámara** (2026-08-22, estilo Unity): el plano rojo (pared YZ) tapaba al verde (suelo XZ) desde ciertos ángulos → el verde no se podía seleccionar. Fix (Unity-style): cada cuadradito de área se (a) offsetea a lo largo de su NORMAL hacia el lado de la cámara (`on = sign(dot(camDir, n))`) Y (b) se extiende en sus dos ejes internos **hacia la cámara** (`su = sign(dot(camDir,u))`, `sv = sign(dot(camDir,v))`), ocupando octantes distintos alrededor del origen ("el rincón que te enfrenta"). Se aplica igual en `Draw` y `Pick` (hit-test coincide con lo visual). El drag sigue usando el plano matemático real (normal del eje bloqueado por `startPos`); el offset es solo visual/picking. Verificado por el usuario (girar cámara: los 3 planos quedan visibles y seleccionables).
- [x] **Panel DBG + grabador de gizmo/objeto** (2026-08-22, `editor/src/UI/GizmoLogPanel.h/.cpp`): panel dockeable "DBG" con botón rojo "record gizmo log" / "recording...". Mientras graba, escribe a `<configDir>/LeirEngine/records/record_gizmo_log.txt`. **Solo registra EVENTOS DE INPUT REALES** (mouse move/click/wheel, teclado, char) vía hooks del `EventQueue` (`Add*Hook`, que coexisten con el canvas que usa `Set*Hook`): si no hay input, no se escribe nada (como el log de consola). Por cada evento escribe la línea del evento + el estado completo: cámara, gizmo (tool T/R/S, space G/L, hover/drag handle vía `TransformGizmo::GetHoverName()/GetDragName()`) + objeto seleccionado (pos/rot/scale euler). Herramienta de diagnóstico para bugs de picking. `EventQueue` ahora soporta múltiples hooks por tipo (vectores) — `Set*Hook` reemplaza la lista, `Add*Hook` agrega. Registrado como panel dockeable y agregado a `kDebugIds` del default layout.
- [x] **Bug post-fix: los planos se cruzaban entre sí + hover robado por las flechas** (2026-08-22): (1) el offset PER-CUADRADO por su propia normal (`p0 = center + n*on*off`) hacía que cada plano partiera de una esquina distinta → se entrecruzaban cerca del origen (el azul/pared atravesaba al rojo). Fix: los 3 cuadrados comparten la MISMA esquina en `g.center` (p0 = center, sin offset por normal individual) y solo se extienden hacia la cámara en sus ejes internos → se encastran como cubo de 3 lados sin cruzarse. (2) al reorientarse los planos, la flecha perpendicular se proyectaba dentro del quad con ~0px y ganaba el empate del hover (las flechas se testeban primero). Fix: en `Pick` para Translate los planos se testean PRIMERO; si el cursor está dentro de uno, gana el plano más cercano a la cámara (profundidad del hit rayo↔plano) y retorna directo. Las flechas solo compiten cuando el mouse no está sobre ningún plano.

### Cierre — extras de cámara y atajos (2026-08-22)
- [x] **Fix del salto de cámara al rotar >90°** (gimbal lock por alias de Euler): `Quaternion::ToEuler` (= `glm::eulerAngles`) devuelve `(160, 80, 180)` para `Euler(-20, 100, 0)` (yaw→180−yaw, roll→±180); el sync inverso `escena→EditorCamera` inyectaba ese roll=180 y corrompía la cámara. Fix: `EditorCamera::SetFromRotation(rot)` — inversa exacta de `GetRotation()` (Ry·Rx, roll=0) vía forward (`pitch=asin(fwd.y)`, `yaw=atan2(−fwd.x,−fwd.z)`). El `CameraTestPanel` usa la misma descomposición (`RollZeroEuler`).
- [x] **E/Q y paneo (middle mouse) usan el UP de la cámara**: nuevo `EditorCamera::GetUp()` (`rot * Up()`); E sube / Q baja a lo largo del up de cámara; el paneo se mueve sobre el plano UP/RIGHT de la cámara (horizontal en `GetRight()`, vertical en `GetUp()`) — como Unity.
- [x] **Q togglea Global/Local** (solo Translate/Rotate; en Scale se ignora), con las mismas guardas que W/E/R (no al volar cámara, no escribiendo, no durante drag).

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