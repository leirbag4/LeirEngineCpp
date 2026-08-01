# TODO: Refactor de descriptors del viewport (ownership en RenderTexture)

Deuda técnica pendiente de la Fase 5/6 de `TODO_RESIZE.md`. Estado: **IMPLEMENTADO y
VERIFICADO en Windows** (2026-07-31).

---

## Problema que resuelve

El viewport 3D se renderiza a un `RenderTexture` y esa imagen se muestrea en la UI como
textura. Para eso Vulkan necesita un **descriptor set** (combined image sampler) por
render target. Hoy ese descriptor lo administra `UIRenderer`:

- Se crea *lazy* en `GetOrCreateVpDescSet(RenderTexture*)` y se cachea en un mapa
  `m_VpDescCache` (`RenderTexture* → VkDescriptorSet`) — **propiedad de `UIRenderer`**.
- Al hacer resize del viewport, la imagen interna se destruye y recrea (`RenderTexture::Resize`)
  → el descriptor viejo apunta a una image view muerta. El editor debe acordarse de llamar
  `InvalidateViewportDescriptor(rt)` (`vkFreeDescriptorSets` + erase) en cada resize
  (`editor/src/main.cpp:561`).
- El pool de la UI (`m_DescPool`, 64 sets) se comparte entre texturas normales y viewports,
  con `FREE_DESCRIPTOR_SET_BIT` solo para poder liberar los sets de viewport.

**Riesgos / fricción del modelo actual:**

1. **Sincronización por convención, no por diseño**: si alguien resiza un RT y no llama
   `InvalidateViewportDescriptor`, el descriptor queda stale → crash/captura de imagen muerta.
2. **No escala al docking futuro**: cada viewport (docking va a multiplicar los RTs) agrega una
   entrada a cache manual + un punto donde hay que acordarse del invalidate.
3. **Pool compartido frágil**: texturas + viewports compiten por los 64 slots del pool de la UI.

## Solución: ownership del descriptor set en `RenderTexture`

`RenderTexture` es el dueño natural: ya depende de `VulkanDevice`, ya destruye/recrea sus
imágenes en `Resize()` y ya hace `vkDeviceWaitIdle` antes de tocar recursos. El RT pasa a
crear, escribir y destruir su propio descriptor set:

- **Ctor**: layout (binding 0 sampler, fragment) + pool (1 set) + alloc + write.
- **Resize()**: re-write del descriptor con la nueva image view (el pool/layout no dependen
  del tamaño; el set se puede actualizar con `vkUpdateDescriptorSets`, ya estamos bajo
  `vkDeviceWaitIdle`).
- **Dtor**: destroy pool (libera el set) + destroy layout.

La UI solo consulta `rt->GetDescriptorSet()`. El layout del RT es **estructuralmente idéntico**
al de `UIRenderer` (binding 0, combined image sampler, fragment) → los descriptor sets se pueden
bindear al pipeline de la UI sin problema (compatibilidad de layouts = igualdad estructural de
bindings, no del handle).

**Beneficios:**
- El descriptor no puede quedar stale: la corrección es inherente al ciclo de vida del RT.
- Se eliminan `m_VpDescCache`, `GetOrCreateVpDescSet` e `InvalidateViewportDescriptor`.
- El pool de la UI vuelve a ser solo de texturas (se puede quitar `FREE_DESCRIPTOR_SET_BIT`).
- Preparado para docking: cada viewport con su propio pool, cero coordinación global.

---

## Fases

### Fase 1 — `RenderTexture` posee su descriptor set ✅
- `RenderTexture.h`:
  - Nuevo público: `VkDescriptorSet GetDescriptorSet() const`.
  - `GetDescriptorInfo()` pasa a ser privado (solo lo usa el propio RT).
  - Privados: `CreateDescriptorResources()`, `UpdateDescriptor()`, `DestroyDescriptorResources()`;
    miembros `m_DescSetLayout`, `m_DescPool`, `m_DescriptorSet`.
- `RenderTexture.cpp`:
  - Ctor: `CreateDescriptorResources()` tras `CreateResources()`.
  - `Resize()`: tras recrear recursos → `UpdateDescriptor()` (re-write con la nueva image view).
  - Dtor: destroy descriptor resources + resources (bajo `vkDeviceWaitIdle`).

### Fase 2 — `UIRenderer` sin cache manual ✅
- Eliminar `GetOrCreateVpDescSet`, `InvalidateViewportDescriptor` y `m_VpDescCache`.
- En `Flush`, los viewports usan `m_ViewportDraws[i].texture->GetDescriptorSet()`.
- Pool de la UI sin `FREE_DESCRIPTOR_SET_BIT` (ya no se liberan sets de viewport; queda solo
  para texturas).

### Fase 3 — Editor sin invalidación manual ✅
- `editor/src/main.cpp:560-561`: eliminar la llamada a `InvalidateViewportDescriptor` en
  `UpdateViewportRenderTarget()` (el `Resize` del RT ya re-escribe su descriptor).

### Fase 4 — Verificación ✅ (2026-07-31)
- Build completo OK (engine DLL + editor EXE).
- Arranque normal: sin errores/validation de descriptor.
- Resize real de la ventana (SetWindowPos 1920x991 → 1857x953): `Viewport resized to 897x732
  (1121x915 physical)` sin crash ni VUID — el descriptor se re-escribió en `Resize()`.
- El pool de la UI volvió a ser solo de texturas (sin `FREE_DESCRIPTOR_SET_BIT`).
- (Futuro) docking: cada viewport con su propio pool; cero coordinación global.

---

## Referencias de código

- `engine/include/LeirEngine/Rendering/RenderTexture.h` / `engine/src/Rendering/RenderTexture.cpp`
- `engine/include/LeirEngine/UI/UIRenderer.h` / `engine/src/UI/UIRenderer.cpp`
  - `GetOrCreateVpDescSet` (cpp:161-188), `InvalidateViewportDescriptor` (cpp:190-198),
    flush de viewports (cpp:282-290).
- `editor/src/main.cpp` — `UpdateViewportRenderTarget()` (542-571), llamada al invalidate (561).
- `engine/src/Rendering/VulkanDevice.cpp` — `CreateDescriptorSetLayout` (1005), `CreateDescriptorPool` (1011).
