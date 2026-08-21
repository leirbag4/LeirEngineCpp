# Grid LOD — Estado final y plan

El LOD del grid del editor (`editor/src/Grid/EditorGrid.cpp`) está terminado y
validado por el usuario. Abajo queda el historial de los 7 fixes. Este bloque
es el resumen del modelo **actual**.

## Estado final (modelo actual, 2026-08-20)

- **Densidad por view depth**: `pxPerUnit = scale / w` con `w = clip.w`
  (profundidad de vista), rotación-correcta en todo pitch (a -90° todos los
  puntos del piso comparten profundidad = altura de cámara).
- **Rol por nivel, una vez por frame**: `ComputeLevelRole(spacing, refDensity)`
  con `refDensity = scale / alturaDeCámara` (invariante a rotación) elige el
  estilo (ancho/color/alpha) de cada nivel por bandas de `cellRef =
  spacing * refDensity`:
  - FINE (fina/delgada): `cellRef ∈ [fadeStart, 10·fadeStart)`.
  - CHUNK (gruesa): `cellRef ∈ [10·fadeStart, 100·fadeStart)` — UN solo nivel
    más grueso que el fino, delimita los grupos de 10×.
  - Fuera de bandas no se dibuja. Bordes con smoothstep → crossfade suave.
  - `EmitAllLevels` emite ambos sentidos por nivel y saltea niveles inactivos
    (`levelAlpha < 0.02`).
- **Fade por pixel (fog by depth, shader)**: cada línea es **UN quad** (sin
  subdivisión CPU). El fragment shader disuelve la opacidad con la profundidad
  de vista interpolada (`depth` varying del VS): `alpha = role.alpha *
  min(fadeCelda, fadeHorizonte)` donde `fadeCelda = smoothstep(fadeStart,
  fadeEnd, spacing * density)` con `density = (override >= 0) ? override :
  scale/depth`, y `fadeHorizonte = 1 - smoothstep(horizonStart, horizonEnd,
  depth)` (knobs del Test2). Los valores vienen por push constants
  (Vertex|Fragment); `spacing` viaja por vértice (0 = línea opaca, los ejes).
  El estilo es constante a lo largo de la línea (nunca cambia de gruesa a fina a
  mitad de recorrido). En CPU solo se saltean las líneas totalmente apagadas
  (`wFar < wNear`, clamp a `wMax` y `horizonEnd`).
- **Finos/altos clampados**: el nivel más fino (1u) nunca pierde su banda fina
  y el 10u (su pareja chunk) nunca pierde su banda chunk — no existe 0.1u para
  hacer el handoff. Sin esto el grid desaparecía a cámara muy baja (camH≈2).
- **Arquitectura**: generación 100% CPU por frame (grid infinito re-centrado en
  el XZ de la cámara) + un solo draw call; la expansión a ancho de píxel
  constante y el fade por profundidad los hacen los shaders (`Grid.vert/frag`).
  Vertex buffer doble buffer, clip near-plane + sort far-to-near en CPU (depth
  write off). **~300-600 líneas** (1 quad/line) → sin stutter.
- Knobs en Test2: `px/unit` (modo manual, -1 = cámara), `fadeStart/fadeEnd`,
  `thickWidth` (default 0.9 px), `horizonStart/horizonEnd`. HUD: LOD
  fine/chunk, camH, ref px/u, fade, horizon, `role 1u/10u/100u/1000u`, líneas.

## Historial de fixes (2026-08-20, ver detalle abajo)

1. Rol chunk por celdas propias (no s/10) → sin banda vacía ni alternancia.
2. Densidad por clip.w (no euclidiana) + muestreo denso → verticales a pitch rasante.
3. Bandas de rol (fino + un solo chunk) → sin chunks dobles por cuadrante.
4. Desacople rol vs. densidad puntual → sin "gruesa que se corta" ni estilos que
   cambian con la dirección.
5. `wFar <= wNear` → `wFar < wNear` → las líneas perpendiculares (profundidad
   constante) ya no se descartan.
6. Handoff de banda fina solo para niveles con vecino más fino → grid visible al arrancar (camH≈2).
7. Roll-off de banda chunk solo para niveles con fino real → chunk 10u visible a cámara baja.
8. Horizon fade (knobs `horizonStart/horizonEnd`) → las 100u/1000u se desvanecen antes del far plane, sin corte brusco en el horizonte.
9. **Fog by depth (Fix 9)**: el fade se movió al fragment shader (por pixel, con la profundidad interpolada) → cada línea es 1 quad, sin subdivisión CPU → count ~300-600 (era ~3000 a cámara baja) y sin stutter.
10. Paridad de backends (Fix 10): Vulkan `dstAlphaBlendFactor` ZERO→ONE_MINUS_SRC_ALPHA (gris por alpha del RT), `.wgsl` del grid actualizados (fog), ejes sort last (spacing==0).

## Problemas reportados por el usuario (2026-08-20)

1. **Patrón no uniforme de las líneas chunk a pitch poco profundo** (rotX -30,
   cámara en Y=10): las primeras líneas 10u se ven gruesas/blancas, la 3ª se ve
   ~10x más débil, luego una banda "vacía", luego la línea 100u **súper blanca**
   al lado de las 10u débiles (alternancia débil/fuerte que se repite cada 100u),
   y a la lejanía líneas débiles con una súper blanca cada ~10 — "no tiene
   sentido, hay un error importante ahí".
2. **Las líneas verticales desaparecen a pitch rasante** (rotX → 0.7): tanto las
   gruesas blancas que dividen chunks como las finitas.

## Causas raíz

### Fix 1 (APLICADO, 2026-08-20)

1. **Rol chunk por sub-celdas** (`DensityAlpha((spacing/10) * pxPerUnit)`):
   la línea 10u perdía su borde grueso blanco (0.9 / 3px) en cuanto las celdas
   1u dejaban de ser legibles (densidad < 15 px/u, z≈54), degradándose a línea
   plana gris (0.35 / 1.5px). Eso creaba la banda vacía + la 100u súper blanca
   al lado + la alternancia. **Fix**: el rol chunk ahora depende de las **celdas
   propias** (`DensityAlpha(spacing * pxPerUnit, ...)`). Cada nivel mantiene un
   rol fijo (1u = fina; 10u/100u/1000u = chunk) y se desvanece por su propia
   densidad, sin handoff ni alternancia.
2. **Densidad por distancia euclidiana** en `PxPerUnit`: usaba
   `glm::length(cameraPos - groundPt)`, que es incorrecta a pitch rasante (la
   celda en el borde inferior de pantalla está forshortenada → se ve grande, no
   chica). **Fix**: usar **view depth** (`clip.w`, el w de la proyección =
   distancia a lo largo del eje de la cámara). Además da la densidad **uniforme**
   correcta en pitch -90° (todos los puntos del piso comparten depth = altura).
3. **Muestreo pobre** en `LinePxPerUnit` (`{camPerp, camPerp ± window/2}`): a
   pitch rasante el punto central (z=camZ) queda detrás del near plane → devuelve
   0 → el máximo caía en una muestra lejana → la densidad colapsaba y las
   verticales (paralelas a Z) desaparecían. **Fix**: muestreo denso centrado en
   la perpendicular (`{±0.9, ±0.5, ±0.15, ±0.05, 0}·window`, 9 puntos) que
   siempre abraza la entrada al near plane.
4. **Paridad modo manual/cámara**: el modo manual (knob px/unit del Test2) ahora
   llama el mismo `EmitLevel(spacing, ..., densityOverride)` en loop sobre
   `kLevelSpacings`. Se **eliminó** `EmitUniformLevels` (duplicaba la lógica con
   un crossfade L/10L/100L que ya no aplica al modelo de roles fijos).

**Verificación**: Y=150 cenital = solo 2 niveles (sin micro-cuadraditos 1u);
rotX -30 / Y=10 = 10u uniformes gruesas que se desvanecen suavemente, sin banda
ni alternancia; rotX 0.7 = verticales visibles.

### Fix 2 (APLICADO, 2026-08-20)

**Fade por longitud de línea.** Cada línea ahora se subdivide en `kSeg = 16`
segmentos en espacio-w (view depth, `clip.w`, que es **LINEAL** a lo largo de
la línea) y cada segmento toma la densidad `scale/w` en su punto medio → la
línea es brillante cerca de la cámara y se desvanece hacia el horizonte, en
**ambas orientaciones**. Arregla el bug: a pitch -35 / Y=90 las verticales 1u se
veían pero las horizontales no (el modelo viejo daba UN solo alpha por línea
leído en su punto más cercano; las verticales tomaban el punto bajo la cámara y
quedaban brillantes hasta el horizonte).

Detalles de la implementación:
- **Intervalo visible contiguo**: `[max(min(w0,w1), kNear), min(max(w0,w1),
  wMax)]` con `wMax = scale*spacing/m_FadeStartPx` (más allá ambos roles son
  invisibles → no se emite) y `wFull = scale*spacing/m_FadeEndPx`.
- **Shortcuts**: (a) si `wFar <= wFull` todo el tramo visible está por encima
  de fadeEnd px → 1 solo draw (sin subdivisión); (b) si `|w1-w0| < 1e-6` (línea
  de profundidad constante, ej. horizontal de cara a la cámara, el caso común)
  → 1 draw a esa profundidad (subdividir en w no tendría sentido).
- **Mapeo w→mundo**: `t = (w - w0)/(w1 - w0)`, `a/b = mix(p0, p1, t)`; la
  densidad del segmento usa el punto medio del tramo. En líneas de profundidad
  constante `t0=0, t1=1` (el span completo).
- **Modo manual** (knob px/unit Test2): densidad uniforme, 1 draw por línea, sin
  clamp de `wMax` (la densidad es la del knob, no la del clip.w).
- **Grosor chunk**: `kMajorWidth` 3.0 → **1.5 px** (pedido del usuario: líneas
  de chunk a la mitad).
- Se eliminaron `PxPerUnit` y `LinePxPerUnit` (quedaron sin uso tras el rewrite
  de `EmitLevel`; se conserva `ProjectionScale`, y `kNear` ahora es constante
  de namespace). La guía conceptual de la densidad `scale/w` quedó como comentario
  arriba de `EmitLevel`.

**Verificación**: Y=90/rotX=-35 simétrico (verticales y horizontales 1u
consistentes), rotX=-30 sin banda ni alternancia, líneas chunk más finas.

### Fix 3 (APLICADO, 2026-08-20)

**Roles por bandas: solo 2 niveles visibles por profundidad + knob de grosor.**
El usuario reportó que en cenital (Y≈30) las líneas 10u **cercanas** seguían
gruesas/blancas en vez de pasar a finas y dejar que las 100u delimiten los 10
grupos de 10u (en el horizonte sí se veía bien). Causa: el modelo "todo nivel
≥10 legible = chunk" hacía chunk a las 10u Y a las 100u a la vez en el cuadrante
cercano. Ahora el rol se elige por el tamaño de celda del propio nivel
(`cellPx = spacing * density`) con bandas que cruzan con smoothstep:
- **FINE (minor)**: `cellPx ∈ [fadeStart, 10·fadeStart)` — la línea fina/suave.
- **CHUNK (major)**: `cellPx ∈ [10·fadeStart, 100·fadeStart)` — UN solo nivel
  más grueso que el fino, delimita los grupos de 10×.
- Fuera de esas bandas no se dibuja (más fino que fine, o 2+ niveles más grueso).

En Y=30: 10u (`cellPx≈150`) → fina; 100u (`cellPx≈1500`) → chunk. En zoom
cercano: 1u fina + 10u chunk. La transición entre chunks (10u→100u) cruza de
forma suave porque las rampas de los bordes de banda son complementarias — sin
pop, sin banda vacía ni alternancia.

**Knob `thickWidth:` en Test2** (pedido del usuario): `EditorGrid::SetChunkWidth`
/ `GetChunkWidth` (`m_ChunkWidth`, default 1.5 px), el ancho del chunk se calcula
`kMinorWidth + (m_ChunkWidth - kMinorWidth) * chunkVis`. El HUD muestra `thick
%gpx`. Se eliminó la constante `kMajorWidth` (el default vive en `m_ChunkWidth`).

**Verificación**: cenital Y=30 → 10u suaves + 100u gruesas delimitando los 10
grupos de 10u, sin chunks dobles; knob `thickWidth` ajusta el grosor en vivo.

### Fix 4 (APLICADO, 2026-08-20)

**Desacople rol vs. fade (el bug de "depende de para dónde miramos").** El fix 3
calculaba el rol (fina/chunk, es decir ancho+color) **por segmento** con la
densidad del punto (`cellPx = spacing * density`), y la densidad varía a lo
largo de la línea y con la dirección de la cámara → (a) una línea chunk se
volvía fina a mitad de recorrido ("gruesa que se corta y sigue la de abajo con
unión perfecta" — es la misma línea con dos estilos) y (b) el conjunto de
líneas gruesas cambiaba al girar yaw/pitch ("aparecen y desaparecen").

**Fix**: el rol ahora se calcula **una vez por nivel por frame** desde la
densidad de referencia **rotación-invariante** (`refDensity = scale / altura de
cámara`, el mismo valor que ya mostraba el HUD como `ref px/u`):
- `cellRef = spacing * refDensity` → bandas minor/chunk igual que en Fix 3,
  `levelAlpha`, `levelWidth`, `levelColor`; si `levelAlpha < 0.02` se saltea el
  nivel entero.
- `emitSegment` queda solo con el **fade de alpha** por segmento:
  `alpha = levelAlpha * DensityAlpha(spacing * density, fadeStart, fadeEnd)`.
  El ancho/color es constante a lo largo de la línea.

Resultado: en Y=30 las 100u se mantienen gruesas en toda su longitud y solo se
desvanecen; girar la cámara no cambia estilos (solo el fade se mueve con la
distancia); el LOD transiciona suavemente al subir/bajar la cámara.

**Readout del HUD** (pedido del usuario, para depurar juntos): nueva línea
`role 1u:%.2f 10u:%.2f 100u:%.2f 1000u:%.2f` (alpha de visibilidad por nivel,
índice = log10(spacing)). Debe ser estable al girar la cámara y solo cambiar al
cambiar la altura — si se mueve con yaw/pitch, hay un bug de dirección.

**Verificación**: Y=30 cenital = 10u suaves + 100u gruesas sin cambio de estilo
en la línea; rotX=-30/Y=10 = 1u fina + 10u chunk uniforme; girar yaw no cambia
el readout; rotX≈0.7 rasante = ambas orientaciones consistentes.

### Fix 5 (APLICADO, 2026-08-20) — el descarte de líneas de profundidad constante

**Bug reportado**: "según para dónde miremos las gruesas desaparecen y líneas
aparecen/desaparecen" + "gruesa que se corta y sigue la de abajo" + parpadeo al
mover la cámara. En yaw=0 solo se veían líneas **verticales**; en pitch
exactamente -90 desaparecían TODAS; en rotX=-89 (casi perpendicular) aparecían
todas (432) — "un pelín inclinado arregla todo".

**Causa raíz**: `EmitLevel` descartaba con `if (wFar <= wNear) continue;`. Cuando
`wFar == wNear` la línea tiene profundidad **constante a lo largo de sí misma**
(está perpendicular a la vista), y era descartada aunque estuviera perfectamente
visible (w < wMax). Ocurre exactamente cuando el forward de la cámara no tiene
componente en el eje de la línea:
- horizontales (paralelas a X) con yaw=0: forward_x = 0 → profundidad constante
  → **siempre descartadas** → solo se veían verticales;
- verticales con yaw≈90 / pitch -90 exacto: igual → todo desaparece;
- el parpadeo al volar: el ruido de float hace que `w0`/`w1` alternen entre
  "exactamente iguales" (descartada) y "levemente distintos" (dibujada);
- rotX=-89: ninguna línea es perpendicular exacta → profundidad varía → se dibujan.

**Fix**: `if (wFar < wNear)` (estricto). Una línea de profundidad constante y
visible cae en el guard existente de profundidad constante (un solo segmento,
`density = scale/w`). Solo se descartan las que están detrás del near plane o
pasadas del fade-out. Un cambio de una letra, valida todo el caso de
orientaciones perpendiculares.

**Pendiente (opcional, preguntar al usuario)**: grid invisible al abrir el
editor (camH≈2, lines=2) — la banda "fina" del nivel 1u se sale al roll-off de
handoff (`cellRef=289` en la zona muerta [10·fadeStart, 100·fadeStart) sin
nivel más fino que la reemplace). Fix propuesto: no aplicar el factor de
handoff `(1 - DensityAlpha(cellRef, 10·fadeStart, 10·fadeEnd))` al nivel más
fino generado (spacing == 1).

### Fix 6 (APLICADO, 2026-08-20) — grid invisible a cámara muy baja (arranque)

**Bug**: ni bien se abre el editor (camH≈2, ref 289.11 px/u) solo se veían los
ejes (lines=2). Causa: la banda "fina" del nivel 1u se sale al roll-off de
handoff — `cellRef(1u) = 289` cae en la zona muerta [10·fadeStart,
100·fadeStart) = [150, 1500) donde 1u ya es "demasiado grueso para ser fino"
pero NO existe nivel más fino (0.1u) que tome el rol → `levelAlpha(1u) ≈ 0.006
< 0.02` → nivel 1u descartado, y 10u también (`levelAlpha ≈ 0.008`) → grid vacío.

**Fix**: el handoff de la banda fina (el factor `1 - DensityAlpha(cellRef,
10·fadeStart, 10·fadeEnd)`) solo aplica a niveles que TENGAN un nivel más fino
debajo. Para el nivel más fino generado (spacing == 1) el factor queda en 1: el
grid 1u nunca se apaga por estar demasiado cerca; igual se desvanece
normalmente cuando `refDensity < fadeEnd`. En camH≈2 ahora `levelAlpha(1u) =
0.35` → el grid 1u se ve apenas se abre el editor. Nota: a esa densidad las
líneas chunk 10u siguen sin aparecer (su banda chunk también se sale), pero el
grid fino 1u domina la vista y es lo que se esperaba ver.

**Verificación**: abrir el editor → el grid 1u se ve con la cámara inicial
(camH≈2); zoom out normal → 1u fina + 10u chunk; seguir a la lejanía → 10u fina
+ 100u chunk (sin regresiones del Fix 5).

### Fix 7 (APLICADO, 2026-08-20) — chunk 10u invisible a cámara muy baja

**Bug**: en la posición inicial del editor (camH≈2) y girando la cámara, las
líneas **gruesas** (10u) no aparecían — quedaba el vacío donde deberían estar.
Al subir la cámara aparecían. Mismo patrón que el Fix 6 pero en la banda
**chunk** del 10u: el roll-off superior `(1 - DensityAlpha(cellRef, 100·fadeStart,
100·fadeEnd))` asume que el fino puede ser 0.1u (2 pasos más fino), pero 1u es
lo más fino generado (quedó clampado como fino en el Fix 6) → en camH≈2
`cellRef(10u) ≈ 2890 > 1500` → `levelAlpha(10u) ≈ 0.008 < 0.02` → nivel
descartado entero. El umbral estaba en camH≈3.85 (cuando `cellRef(10u) < 1500`).

**Fix**: el roll-off superior de la banda chunk solo aplica a niveles cuyo fino
tenga vecino más fino real. Para el 10u (pareja chunk del fino clampado 1u) el
factor queda en 1 — simétrico al Fix 6. El roll-off del 100u se mantiene (su
fino 10u sí tiene vecino más fino).

**Verificación**: en la posición inicial girando la cámara → 1u fina + 10u
chunk visibles; camH=10 idéntico a antes (`cellRef(10u)=580 < 1500` → el factor
ya era 1); camH≈38.7 → crossfade 10u fina + 100u chunk intacto.

### Fix 8 (APLICADO, 2026-08-20) — fade de horizonte para niveles gruesos

**Bug**: al superar Y≈30, el nuevo nivel chunk (100u) se veía hasta ultra lejos
y se cortaba de golpe en el horizonte (sin dissolve), formando un "sólido" de
líneas convergentes. Causa: el fade por celdas tiene banda `[scale·s/30,
scale·s/15]` que crece con el spacing; con `scale≈580` el 100u tiene banda
[1935, 3870], pero el **far plane de la cámara es 2000** → el 100u llegaba al
far plane con ~93% de alpha y se recortaba (las 1u/10u ya se habían apagado a
w≈39/387, por eso se veían perfectas).

**Fix (horizon fade, dos knobs nuevos en Test2 — no hardcoding)**: el fade final
por segmento pasa a `min(fadeCelda, fadeHorizonte)` con `fadeHorizonte =
1 - DensityAlpha(w, horizonStart, horizonEnd)`. Knobs `horizonStart:`/
`horizonEnd:` (profundidad de vista absoluta, defaults 1000/1800; deben quedar
debajo del far plane 2000). Además: clamp de `wFar = min(wFar, wMax,
horizonEnd)` en modo cámara (concentra los segmentos en las bandas de fade) y
el shortcut "fully bright" ahora exige `wFar <= wFull && wFar <= horizonStart`.
El modo manual (px/unit) no aplica el fade horizonte (test de LOD puro).

**Verificación**: Y≈40 (10u fina + 100u chunk) → las 100u se desvanecen
suavemente antes del horizonte, sin corte brusco ni sólido; 1u/10u idénticas;
knobs ajustan la banda en vivo (HUD muestra `horizon %.0f..%.0f`).

### Fix 9 (APLICADO, 2026-08-20) — fog by depth: el fade pasa al shader

**Bug**: al arrancar el editor (camH≈2, vista oblicua) el count de líneas era
**~3091** y el viewport se veía "trabado" (spikes de frame con 60 FPS). Causa:
el fade por longitud se computaba en CPU **por segmento** (`kSeg=16`); cada
línea que cruzaba una banda de fade se dividía en 16 quads → ~3091 líneas ×
(~18.5k vértices y ~1MB de memcpy por frame en Debug). En top-down eran ~220
(profundidad constante → 1 segmento) — por eso el "300" que se recordaba. El
cambio se introdujo en `a902d66` (fade por longitud), NO en el Fix 8.

**Fix (fog by depth)**: el fade se movió al **fragment shader** — cada línea es
**1 quad** (sin subdivisión CPU) y el pixel disuelve su opacidad con la
profundidad de vista interpolada (`depth` varying = `lerp(clipS.w, clipE.w,
cornerX)`, consistente en Vulkan/D3D12/WebGPU — no `SV_Position.w`, que en
SPIR-V es el recíproco):
- `Grid.vert.slang`: VSOutput + `depth`/`spacingOut`; VSInput + `spacing`
  (location 6, stride 64); push block ampliado (8 floats).
- `Grid.frag.slang`: `alpha = color.a * min(smoothstep(fadeStart, fadeEnd,
  spacing*density), 1 - smoothstep(horizonStart, horizonEnd, depth))` con
  `density = (overrideDensity >= 0) ? overrideDensity : scale/depth`.
  `spacing <= 0` (ejes) o modo manual → sin fade de cámara.
- CPU (`EmitLevel`): eliminados `kSeg`, `wFull`, emitSegment, constant-depth
  guard y fully-bright shortcut (los subsume el shader). Solo saltea líneas
  totalmente apagadas (`wFar < wNear`, clamp a `wMax`/`horizonEnd`) y emite 1
  quad con `color.a = role.alpha` + `spacing`.
- `Render()`: push de `scale`, fade/horizon, overrideDensity (mask
  `Vertex|Fragment`); atributo loc 6; fallback de pipeline con push range
  Vertex|Fragment.

**Resultado**: count ~300-600 (1 quad/line) → sin stutter, fade más liso
(per-pixel exacto, no 16 pasos). Los knobs siguen igual (ahora alimentan los
push constants). Verificado por el usuario: 1u/10u/100u suaves, ejes opacos
(spacing=0), count normal al arrancar.

### Fix 10 (APLICADO, 2026-08-21) — paridad de backends: Vulkan gris, WebGPU sin fog, ejes tapados

Reportado por el usuario: D3D12 perfecto, pero **Vulkan** mostraba las líneas
convergentes como un bloque gris y los ejes rojo/azul tapados con huecos, y
**WebGPU** no tenía nada de fog. Tres causas raíz:

1. **Vulkan gris — blend de ALPHA inconsistente** (`VulkanDevice.cpp`): el
   `dstAlphaBlendFactor` era `VK_BLEND_FACTOR_ZERO` mientras D3D12 y WebGPU usan
   `ONE_MINUS_SRC_ALPHA`. El UI compone el viewport con `texColor * fragColor`
   (usa el **alpha del RT**); con `ZERO` cada línea tenue sobrescribía el alpha
   del RT → el viewport quedaba semi-transparente → el fondo oscuro de la UI se
   filtraba por las líneas desvanecidas → "gris". **Fix**: `dstAlphaBlendFactor =
   ONE_MINUS_SRC_ALPHA` (igual que D3D12/WebGPU → el alpha del RT queda ~1 →
   viewport opaco). Se corrige para todos los pipelines blendeados de Vulkan.
2. **WebGPU sin fog — `.wgsl` viejos** (`engine/shaders/Grid.vert.wgsl` +
   `Grid.frag.wgsl`): eran de antes del fog (push de 4 floats, sin `spacing`/
   `depth`, sin fade). El backend WebGPU carga estos `.wgsl` a mano (el runtime
   NO usa el export WGSL de Slang — ver Deuda técnica). **Fix**: actualizados
   espejando el `.slang` (spacing loc 6, `depth` varying, push 8 floats,
   smoothsteps, `spacing<=0` opaco).
3. **Ejes tapados — sort al revés** (`EditorGrid::Render`): el sort es ascendente
   por `key = min(clip.w)` → el más cercano se dibuja primero → el más lejano
   queda arriba. Los ejes (cerca de la cámara) quedaban debajo de las líneas
   lejanas en los cruces → "huecos". **Fix**: los ejes opacos (`spacing == 0`)
   reciben `key = FLT_MAX` → se ordenan últimos → siempre arriba.

**Verificación**: los 3 backends con el fog per-pixel suave, sin gris en Vulkan,
sin huecos en los ejes; count ~300-600.

## Archivos relevantes

- `editor/src/Grid/EditorGrid.cpp` — `GenerateLines` (entry, unifica manual/
  cámara), `EmitAllLevels` (loop por nivel + debug alphas), `ComputeLevelRole`
  (rol por bandas, 1× por frame), `EmitLevel` (1 quad por línea + skip de
  apagadas), `Render` (clip near-plane + sort far-to-near + push constants).
- `editor/src/Grid/EditorGrid.h` — `Line`, `LevelRole`, `GridVertex` (+spacing,
  stride 64), `GridPushConstants` (8 floats), `DrawLine`, API de fade
  (`SetFadeThresholds`) + `SetChunkWidth` + `SetHorizonFade` + readout
  `GetDebugLevelAlpha`.
- `engine/shaders/Grid.vert.slang` — VSInput +`spacing` (loc 6), VSOutput
  +`depth`/`spacingOut` (view depth interpolada), push block ampliado.
- `engine/shaders/Grid.frag.slang` — fade por pixel (fog by depth): `min(fadeCelda,
  fadeHorizonte)` con smoothsteps, `spacing <= 0` = opaco (ejes).
- `engine/shaders/Grid.vert.wgsl` / `Grid.frag.wgsl` — espejos del `.slang` para
  el backend WebGPU (runtime carga estos a mano; ver Deuda técnica del cableado).
- `engine/src/Rendering/VulkanDevice.cpp` — `dstAlphaBlendFactor`
  `ONE_MINUS_SRC_ALPHA` (paridad con D3D12/WebGPU).
- `editor/src/UI/GizmoLineTestPanel.h/.cpp` — panel Test2 (knobs px/unit,
  fadeStart/fadeEnd/thickWidth/horizonStart/horizonEnd).
- `editor/src/main.cpp` — HUD `GridLodDebug` (+ línea `role`/`horizon`), call a
  `m_Grid->Render`.

## Deuda técnica / pendientes

- **Cablear Slang→WGSL al runtime WebGPU (PENDIENTE, acordado con el usuario)**: el
  diseño es "escribir el shader una vez en `.slang` y exportarlo por backend"
  (`ShaderExporter::ExportAll` ya traduce a SPIR-V/DXIL/Metal/WGSL/GLSL450 →
  `shaders_export/`). Pero el **runtime WebGPU no usa ese export**: carga los
  `.wgsl` **escritos a mano** en `engine/shaders/` (copiados verbatim por CMake),
  que se mantienen espejando el `.slang` y **derivan** (por eso el Fix 10 de los
  `.wgsl` del grid quedó atrasado). Para cerrar el diseño single-source hay que:
  1. Al arrancar el editor (junto a `WriteRuntimeSidecars`), exportar el WGSL de
     los shaders a `LEIR_SHADER_DIR` (o que el backend WebGPU lea del export).
  2. Alinear **entry points**: el backend WebGPU hardcodea `vs_main`/`ps_main`
     (WebGPUBackend.cpp:1366/1370); el export de Slang usa `main`. O configurar
     el entry point del pipeline o renombrar en el export.
  3. Alinear el **group/binding del push**: el backend espera `@group(1)
     @binding(0)` para el UBO de push; Slang mapearía `register(b1, space0)`
     distinto. Verificar y alinear.
  4. Validar que el WGSL exportado compile en wgpu-native (naga): el Grid no usa
     bindless, así que es buen candidato; otros shaders (UI/Sprite con
     `binding_array`) tienen casos conocidos que naga no acepta.
  Mientras tanto, los `.wgsl` a mano se mantienen actualizados a mano.
- El modo manual (knob `px/unit`, `densityOverride`) se mantiene a propósito:
  es un andamiaje de testeo útil para verificar la transición LOD sin tocar la
  cámara. Descartable si sobra.
- El math del grid (bandas, densidad, fade) no tiene tests automáticos — vive en
  el editor (no en el target de tests). Diferido; si se quiere, extraer el math
  puro a un header testeable y agregarlo a `tests/`.