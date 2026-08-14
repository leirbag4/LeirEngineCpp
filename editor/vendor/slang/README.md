# vendored libslang (Slang shader compiler) — v2026.14.1

Prebuilt runtime libraries of the **Slang** shader compiler used by the editor's
shader tooling (`IShaderCompiler` impl + multi-format exporter + hot-reload,
Plan A in `TODO_RHI_SLANG.md`). The engine itself never links Slang.

## Layout

```
slang/
├── include/   # slang.h, slang-com-ptr.h, ... (16 headers, identical on all 3 platforms)
├── windows/   # slang-compiler.lib + runtime DLLs (x86_64, MSVC ABI)
├── linux/     # libslang-compiler.so.0.2026.14.1 + runtime .so (x86_64, glibc 2.27+)
├── macos/     # libslang-compiler.0.2026.14.1.dylib + runtime .dylib (arm64)
└── LICENSE.txt  # Apache-2.0 WITH LLVM-exception (upstream)
```

Linked + deployed by `cmake/SlangTooling.cmake` (`leir_setup_slang_target`,
shared by `LeirEngineEditor` and the `SlangExportTest` smoke test).

## Provenance

- Upstream release: `slang-2026.14.1-*` assets of
  [shader-slang/slang](https://github.com/shader-slang/slang/releases/tag/v2026.14.1)
  (published 2026-07-30).
- Vendored: 2026-08-14. Re-download on upgrade and check that `slang.h` still
  has `slang_createGlobalSession2` / `SLANG_API_VERSION` (the APIs the tooling
  uses).
- Naming is the **modern** scheme (`slang-compiler`, `slang-rt`, ...). The
  legacy `slang.dll` / `libslang.so` proxy names are deprecated upstream and are
  intentionally **not** vendored — link `slang-compiler` (+ `slang-rt` if
  referenced).

## Platform notes

- **Windows**: `slang-compiler.lib` is an MSVC import lib (links under MinGW
  too). The 4 DLLs are copied next to the exe by `leir_setup_slang_target`
  (no PATH dependency). `slang-llvm.dll` is NOT required for the editor's
  compile targets (SPIR-V/DXIL/Metal/WGSL/GLSL).
- **Linux**: the `.so` compiler is self-contained (no `libslang-*` in
  `DT_NEEDED`); `glslang`/`glsl-module` are `dlopen`ed, so the whole runtime
  must sit next to the exe (their RUNPATH resolves to the exe dir). Symbolic
  links from the package are not vendored; the fully-versioned files are linked
  directly.
- **macOS**: `install_name` is `@rpath/libslang-compiler.0.2026.14.1.dylib`;
  `leir_setup_slang_target` sets `BUILD_RPATH` to `@loader_path` (+ the engine
  dir) so the dylibs copied next to the exe are found. arm64 (matches
  `macos-latest` runners).

## Upgrading

1. Download the 3 release zips for the new tag into a scratch dir and extract
   (`tar.exe` is much faster than `Expand-Archive` on Windows).
2. Copy `include/` (compare header count first), the `windows` .lib/.dll,
   `linux` .so (fully-versioned names), `macos` .dylib files, and the new
   `LICENSE.txt`.
3. Update the file names in `cmake/SlangTooling.cmake` (link + copy lists).
4. Rebuild + run `SlangExportTest` locally, then on all 3 CI platforms.