# LeirEngine

Motor de juego C++ multiplataforma construido desde cero (Vulkan / D3D12 / WebGPU), con un
**ECS data-oriented propio** (SIMD + multithreading), física Jolt, audio SoLoud y un editor con
sistema de docks.

Esta documentación combina:

- **API Reference** — generada automáticamente desde los headers públicos (Doxygen → Breathe/Exhale).
- **Guías** — escritas a mano en Markdown (en construcción).

```{toctree}
:maxdepth: 2
:caption: API Reference
api/index
```

```{toctree}
:maxdepth: 2
:caption: Guías
guides/ecs/ecs-public-api
guides/ecs/hybrid-ecs
```

```{note}
Más guías en camino (engine, editor, web): ver `TODO_DOCS.md` §3.5.
```