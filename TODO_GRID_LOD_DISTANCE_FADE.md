# Grid LOD — Fade por longitud (Estado y plan)

Documenta el estado del LOD del grid del editor (`editor/src/Grid/EditorGrid.cpp`):
ambos fixes (densidad por clip.w + **fade por longitud de línea**) están
aplicados.

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

## Archivos relevantes

- `editor/src/Grid/EditorGrid.cpp` — `EmitLevel` (rol por nivel + fade por
  segmento), `GenerateLines`, `Render` (expansión a quads + sort).
- `editor/src/Grid/EditorGrid.h` — `GridVertex`, `Line`, `DrawLine`, API de fade
  (`SetFadeThresholds`) + `SetChunkWidth` + readout `GetDebugLevelAlpha`.
- `editor/src/UI/GizmoLineTestPanel.h/.cpp` — panel Test2 (knobs px/unit,
  fadeStart/fadeEnd/thickWidth).
- `editor/src/main.cpp` — HUD `GridLodDebug` (+ línea `role`), call a
  `m_Grid->Render`.