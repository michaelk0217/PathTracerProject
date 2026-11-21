# PathTracer: CSCI 580 Final Project

THIS IS AN AI-GENERATED README OF CURENT STATE OF PROJECT!

AI-Generated codes are labeled by comment.

ALL CMAKE FILES ARE AI-Generated.

Some vulkan codes are re-used from past projects as of now to save time (such as vk::Context, vk::Swapchain, vk::Resources).
Some Design/Architecture choices (Bindless descriptor architecture and Scene<===>GpuScene mirroring/syncing) were made by AI.


A real-time **Vulkan hardware ray tracer** written in C++20 and **Slang**. The end goal is a renderer that runs either **Monte Carlo path tracing** or **ReSTIR**, each with an optional **spectral** mode. This README describes the codebase **as it is today**: the ray tracing foundation those techniques will be built on.
 
> **Current state:** The ray tracer displays textured Sponza and supports Monte Carlo direct lighting in Sponza and Cornell. Sponza also supports a single directional sun. The sample-count comparisons in both scenes use a square area light. Shadow visibility and indirect bounces are future work.
 
---
 
## Table of contents
1. [Quick start](#1-quick-start)
2. [Controls and what you should see](#2-controls-and-what-you-should-see)
3. [Repository layout](#3-repository-layout)
4. [Architecture: the layers](#4-architecture-the-layers)
5. [What every file does](#5-what-every-file-does)
6. [How it works together](#6-how-it-works-together)
7. [CPU ↔ GPU data model](#7-cpu--gpu-data-model)
8. [Shaders](#8-shaders)
9. [Daily workflow](#9-daily-workflow)
10. [Debugging](#10-debugging)
11. [Conventions](#11-conventions)
12. [Extending the renderer](#12-extending-the-renderer)
13. [Vulkan glossary for newcomers](#13-vulkan-glossary-for-newcomers)
14. [Further reading](#14-further-reading)
---
 
## 1. Quick start
 
### Prerequisites
| What | Notes |
|---|---|
| **Windows 10/11** | The project is developed with MSVC on Windows |
| **GPU with hardware ray tracing** | NVIDIA RTX 20-series or newer, AMD RX 6000 or newer, or Intel Arc. Update your driver; Vulkan 1.3 is required. **Without such a GPU the program can't run** (it stops with "No GPU with Vulkan 1.3 + KHR ray tracing found") |
| **Visual Studio 2026 or 2022** | With the **"Desktop development with C++"** workload (includes MSVC, CMake and Git) |
| **CMake ≥ 3.25** | The copy bundled with Visual Studio works. **The VS 2026 preset needs CMake ≥ 4.2** (or the CMake that ships with VS 2026) |
| **Vulkan SDK ≥ 1.4.328** | From LunarG. Provides validation layers, `slangc` (shader compiler) **and the libraries we use**. The installer sets the `VULKAN_SDK` environment variable |
 
### Vulkan SDK optional components (important)
All C++ dependencies come **from the Vulkan SDK**. In the SDK installer, or later via `maintenancetool.exe` in the SDK folder → *Add or remove components*, make sure these are selected:
 
- **Volk** (Vulkan function loader)
- **Vulkan Memory Allocator (VMA)**
- **GLM** (math)
- **SDL** (window and input; SDL3)
If one is missing, CMake stops and tells you which component to install. **Restart Visual Studio or your terminal after installing the SDK**, so it picks up `VULKAN_SDK`.
 
Only **Dear ImGui** is downloaded from GitHub at configure time (it isn't part of the SDK). It's built but **not used yet**. Pass `-DPT_WITH_IMGUI=OFF` to skip it.
 
### Configure, build, run
From the project root, in a terminal:
```bat
cmake --preset vs2026        :: Visual Studio 2026   (or: cmake --preset vs2022)
```
Then open **`build\vs2026\PathTracer.slnx`** (VS 2022: `build\vs2022\PathTracer.sln`). `PathTracer` is already the startup project, so press **F5**.
 
Launch with `--samples COUNT` for the Sponza atrium lighting comparison or add `--cornell` for the Cornell box. Both accept 1 to 1024 light samples per pixel. `--sponza` and `--cornell` default to 16. Use `--sun` or `--sponza --sun` for a single directional sun from the same atrium camera. The sun has a fixed direction and takes no sample-count argument. Launching without arguments opens the original Sponza texture preview.

```powershell
.\build\vs2026\bin\Debug\PathTracer.exe --sun
.\build\vs2026\bin\Debug\PathTracer.exe --samples 1
.\build\vs2026\bin\Debug\PathTracer.exe --samples 16
.\build\vs2026\bin\Debug\PathTracer.exe --cornell --samples 1
.\build\vs2026\bin\Debug\PathTracer.exe --cornell --samples 16
```

| CMake option | Default | Meaning |
|---|---|---|
| `PT_ENABLE_VALIDATION` | `ON` | Enable Vulkan validation layers + debug messages |
| `PT_COMPILE_SHADERS` | `ON` | Compile `.slang` → `.spv` as part of the build |
| `PT_WARNINGS_AS_ERRORS` | `OFF` | Treat compiler warnings as errors (`/WX`) |
| `PT_WITH_IMGUI` | `ON` | Fetch and build Dear ImGui |
 
**"Could not find any instance of Visual Studio"** means the preset doesn't match the Visual Studio you have installed, or the C++ workload is missing. Use the other preset, or run `cmake --help` to see which generators your CMake supports.
 
### Expected console output
```
[info] GPU: NVIDIA GeForce RTX ... (Vulkan 1.4.x)
[info] Renderer: swapchain 1980x1080, 3 images
[info] RT pipeline: 4 groups, handle 32 B, record stride 32 B, SBT 256 B
[info] GpuScene: built 7 BLAS (...)
[info] GpuScene: built TLAS (7 instances, ...)
[info] GpuScene: 7 meshes, 7 instances, 4 materials
```
**There should be no `[vk]` lines.** Those are validation messages, and any `[vk]` error is a bug.
 
---
 
## 2. Controls and what you should see
 
| Input | Action |
|---|---|
| **Right mouse button + drag** | Look around |
| **W / A / S / D** | Move forward / left / back / right |
| **E / Q** | Move up / down |
| **Left Shift** | Move 4× faster |
| **Esc** or close the window | Quit |
 
The default Sponza preview starts near a blue curtain. With `--samples`, the camera looks along the atrium under an added area light. In Cornell, the ceiling panel illuminates the colored walls and the tall box. Keep the camera still to compare noise at different sample counts.
 
---
 
## 3. Repository layout
```
CMakeLists.txt             Root build file: options, includes, module order
CMakePresets.json          Configure presets: vs2026, vs2022, ninja-msvc
cmake/
  Dependencies.cmake       Finds Vulkan, volk, VMA, GLM, SDL3, slangc in $VULKAN_SDK; fetches ImGui
  ProjectOptions.cmake     Warning flags/defines + helpers pt_add_module(), pt_copy_runtime_dlls()
  Shaders.cmake            pt_add_slang_shaders(): compiles .slang -> .spv at build time
shaders/                   ← Slang include root (slangc -I shaders)
  CMakeLists.txt           Which shader files to compile
  shared/GpuStructs.h      Structs + constants shared by C++ AND Slang (the CPU↔GPU contract)
  common/bindings.slangh   Global GPU resources + helpers every shader includes
  integrators/pathtrace_mc.slang   Ray tracing shaders: rayGen, miss, shadowMiss, closestHit
  passes/tonemap.slang     Compute pass: HDR image -> display image (exposure, ACES, sRGB)
src/                       ← C++ include root  (#include "vk/Context.h")
  core/                    Basic types, logging, asserts
  platform/                Window + input (SDL3)
  vk/                      Thin Vulkan layer (device, memory, images, pipelines, AS, ...)
  scene/                   CPU-side scene: meshes, materials, camera, Cornell box
  renderer/                Frame loop, GPU copy of the scene, shared-struct checks
  app/                     main(), App: owns everything, runs the loop
assets/scenes/             (empty) place for future scene files
build/                     Generated by CMake, never committed
```
 
---
 
## 4. Architecture: the layers
 
Each folder under `src/` is its own **static library**. Dependencies only go **downward**, and the linker enforces it: for example, `pt_scene` doesn't link Vulkan, so including a `vk/` header from `scene/` fails to build.
 
```
  app        PathTracer.exe   main loop, input → camera, owns everything
   │
  renderer   pt_renderer      Renderer (frame loop), GpuScene (scene on the GPU)
   ├──────────────┐
  scene          vk           pt_scene: CPU data, no Vulkan     pt_vk: thin Vulkan wrappers, no scene knowledge
   │              │
  platform       │            pt_platform: SDL3 window + input (knows only Vulkan *handle types*)
   │              │
  core ──────────┘            pt_core: types, log, assert, GLM aliases
```
 
**Rules**
1. **Scene and app logic never call Vulkan, and Vulkan code knows nothing about cameras or materials.** `renderer/` is the only bridge between the two.
2. **No abstraction over Vulkan** (no "RHI"). `vk/` is a thin layer of RAII wrappers, and the renderer uses Vulkan concepts directly.
3. **Plain data between layers:** `InputState` (platform → app), `RenderSettings` (app → renderer), `Scene` + dirty flags (scene → renderer).
---
 
## 5. What every file does
 
### `src/core/`
| File | Purpose |
|---|---|
| `Types.h` | `u32`, `vec3`, `mat4`, … aliases (GLM); `Extent2D`; **`NonCopyable`** (base for every class that owns a GPU/OS resource: movable, never copyable) |
| `Log.h/.cpp` | `log::info / warn / error("x = {}", x)` with `std::format` syntax. Also prints to the **Visual Studio Output window** |
| `Assert.h` | `PT_ASSERT(cond, "msg")`: logs and breaks into the debugger. **Active in Release too.** `PT_DEBUG_BREAK()` |
 
### `src/platform/`
| File | Purpose |
|---|---|
| `Window.h/.cpp` | Creates the SDL3 window. `pollEvents()` turns SDL events into an **`InputState`** snapshot (keys, mouse delta) once per frame. Also provides the Vulkan instance extensions SDL needs and creates the **surface** (so `vk/` never depends on SDL) |
 
### `src/vk/`: the Vulkan layer
| File | Purpose |
|---|---|
| `Common.h` | **The one include for Vulkan** in this project: `volk.h` + `vk_mem_alloc.h` + logging. `VK_CHECK(expr)` breaks on any non-success `VkResult`. `kMaxFramesInFlight = 2` |
| `VmaImpl.cpp` | The single file that compiles VMA's implementation (`#define VMA_IMPLEMENTATION`) |
| `Context.h/.cpp` | Creates the **instance** (+ validation layer + debug messenger), picks the **GPU** (requires Vulkan 1.3 + ray tracing extensions), creates the **device** with all needed features, the **queue** (one queue for graphics, compute and present), and the **VMA allocator**. `caps` holds GPU limits like the SBT alignments. `setName()` gives objects debug names |
| `Swapchain.h/.cpp` | The window's images. `acquire()` / `present()` return `false` when the window changed size. One `renderFinished` semaphore **per swapchain image** |
| `Resources.h/.cpp` | **`Buffer`**: VMA memory, always with a **device address** (a GPU pointer), `upload()` for host-visible memory. **`Image2D`**: image + view + **remembers its layout/stage/access**, so `transition(cmd, nextState)` emits the right barrier. `imageBarrier()` / `memoryBarrier()` for everything else. `states::` holds common target states |
| `Bindless.h/.cpp` | **One global descriptor set** used by every pipeline (see §7.4) |
| `Pipeline.h/.cpp` | Load SPIR-V, create shader modules, the **one shared pipeline layout**, compute pipelines, and the **ray tracing pipeline + shader binding table (SBT)** |
| `AccelStruct.h/.cpp` | Builds **BLAS** (triangles of one mesh) and **TLAS** (instances of BLASes) on the GPU |
| `Immediate.h/.cpp` | `submitImmediate(ctx, [](VkCommandBuffer cmd){...})`: record, submit, **wait**. Load-time work only (acceleration structure builds) |
 
### `src/scene/`: CPU data, no Vulkan
| File | Purpose |
|---|---|
| `Scene.h/.cpp` | `Vertex`, `Mesh`, `Material`, `Instance`, the `Scene` container, and **dirty flags** (`DirtyGeometry / DirtyInstances / DirtyMaterials`) that tell the GPU side what changed. Use `addMesh / addMaterial / addInstance`, which set the flags for you |
| `Camera.h/.cpp` | Free-fly camera (yaw/pitch in degrees). `view()`, `projection(aspect)` (**Vulkan clip space, Y flipped**). Knows nothing about input; the app calls `move()` / `rotate()` |
| `SceneLoader.h/.cpp` | `makeCornellBox()`: the procedural test scene. `loadGltf()` is a stub (not implemented) |
 
### `src/renderer/`: the bridge
| File | Purpose |
|---|---|
| `Renderer.h/.cpp` | Owns the swapchain, bindless set, pipeline layout, pipelines, output images and per-frame resources, and runs **`renderFrame()`** (see §6.2) |
| `GpuScene.h/.cpp` | **The GPU copy of `Scene`.** `sync(scene)` uploads whatever the dirty flags say changed: vertex/index buffers, materials, the per-instance mesh table, and rebuilds BLAS/TLAS |
| `RenderSettings.h` | Plain struct the app edits and the renderer reads: `maxBounces`, `exposure`, `vsync` |
| `GpuStructsCheck.cpp` | `static_assert`s on the size of every shared struct (see §7.3) |
 
### `src/app/`
| File | Purpose |
|---|---|
| `main.cpp` | `int main()` → `App app; return app.run();` (`SDL_main.h` makes this work on Windows) |
| `App.h/.cpp` | Creates **Window → Context → Renderer** (destroyed in reverse), builds the scene, runs the loop: poll input → move camera → `renderer.renderFrame(scene, settings)` |
 
---
 
## 6. How it works together
 
### 6.1 Startup
```
App::App()
 ├─ Window              SDL3 window (Vulkan-capable)
 ├─ vk::Context         instance → surface (via Window) → GPU → device + queue → VMA
 ├─ Renderer            swapchain, bindless set, pipeline layout, GpuScene,
 │                      timeline semaphore, 2 frame slots (cmd pool/buffer, semaphore, uniform buffer),
 │                      HDR + display images, tonemap pipeline, ray tracing pipeline + SBT
 └─ makeCornellBox()    CPU scene; all dirty flags set
First renderFrame()
 └─ GpuScene::sync      upload buffers → build 7 BLAS → build TLAS → write TLAS into bindless set
```
 
### 6.2 One frame: `Renderer::renderFrame(scene, settings)`
```
1. GpuScene::sync(scene)            upload anything that changed (usually nothing)
   if TLAS rebuilt → bindless.setTlas()
2. wait timeline ≥ slot's value     the GPU is done with the frame that last used this slot (2 frames ago)
3. swapchain.acquire()              get an image to draw into (out of date → resize, skip frame)
4. record command buffer:
     write FrameUniforms            camera matrices, maxBounces, exposure → per-slot buffer
     push constants                 GPU addresses of frame uniforms, mesh table, materials + image size
     bind global descriptor set     (once for COMPUTE, once for RAY_TRACING: bindings are per bind point)
     HDR image → GENERAL            barrier via Image2D::transition
     vkCmdTraceRaysKHR              rayGen per pixel → closestHit/miss → writes HDR image
     HDR → read, display → write    barrier
     tonemap (compute)              HDR image → display image (exposure, ACES, sRGB)
     blit display → swapchain       blit converts RGBA → BGRA
     swapchain image → PRESENT      barrier
5. vkQueueSubmit2                   wait: imageAvailable · signal: renderFinished[image] + timeline value
6. swapchain.present()
```
 
### 6.3 Synchronization in one picture
```
CPU frame N (slot N % 2)                          GPU
wait timeline ≥ value(slot) ◄──────────────────── signals timeline when frame N-2 finished
acquire ─► imageAvailable ──────────┐
record                              ▼
submit ──────────────────► wait imageAvailable → work → signal renderFinished[i] + timeline
present (waits renderFinished[i]) ◄─────────────────────────┘
```
- **2 frames in flight:** the CPU records frame N+1 while the GPU works on frame N. That's why per-frame resources (command buffer, uniform buffer, `imageAvailable`) exist twice, indexed by **slot**.
- **3 swapchain images** (driver minimum + 1). `renderFinished` is indexed by **image**, not by slot.
- **One timeline semaphore** (a GPU counter) paces the CPU against the GPU. Frame N signals value N+1.
---
 
## 7. CPU ↔ GPU data model
 
### 7.1 Scene → GpuScene
```
Scene (CPU)                                  GpuScene (GPU buffers)
meshes[].vertices ─┐ concatenated ─────────► m_vertices  GpuVertex[]    ← also BLAS input
meshes[].indices  ─┘ (indices stay LOCAL) ─► m_indices   u32[]          ← also BLAS input
instances[]       ─────────────────────────► m_meshInfos GpuMeshInfo[]  ← pc.meshes, indexed by InstanceID()
materials[]       ─────────────────────────► m_materials GpuMaterial[]  ← pc.materials
one BLAS per mesh, one TLAS for the scene  ─► bindless binding 2        ← gTlas
```
- **Indices stay local to each mesh** (0 … vertexCount−1). Each mesh instead gets its own **base address** into the vertex buffer (in `GpuMeshInfo` and in the BLAS input).
- **Instance *i* gets `customIndex = i` in the TLAS**, so `InstanceID()` in a hit shader indexes `pc.meshes[]` directly.
- **Dirty flags** decide what `sync()` does: geometry → re-upload vertices/indices + rebuild BLAS + TLAS; instances → mesh table + TLAS; materials → material buffer. When something changes, `sync()` calls `waitIdle()` first (simple and safe; scene changes are rare).
- **After editing scene data, call `scene.markDirty(...)`**, or use the `add…` helpers. Otherwise the GPU keeps the old data.
### 7.2 Buffers are reached by pointer, not by descriptor
All buffers are created with a **device address**. Shaders get these addresses through **push constants** and use them like C pointers:
```slang
GpuMeshInfo mesh = pc.meshes[InstanceID()];       // pointer → array
uint i0 = mesh.indices[PrimitiveIndex() * 3];      // pointer stored inside a struct
```
 
### 7.3 `shaders/shared/GpuStructs.h`: the contract between C++ and Slang
This one header is compiled by **both** languages:
- In C++ it lives in `namespace pt::gpu` and uses GLM types (`float3 = glm::vec3`).
- `PT_PTR(T)` is `uint64_t` in C++ and `T*` in Slang.
- Both sides use **scalar block layout** (`-fvk-use-scalar-layout` + the `scalarBlockLayout` feature): no hidden padding, so sizes must match byte for byte.
Current contents:
| Struct | Fields | Size |
|---|---|---|
| `GpuVertex` | position, normal, uv | 32 |
| `GpuMaterial` | baseColor, roughness, emission, metallic, ior, transmission, baseColorTexture | 44 |
| `GpuMeshInfo` | vertices*, indices*, materialIndex, triangleCount | 24 |
| `FrameUniforms` | viewInverse, projInverse, cameraPosition, maxBounces, exposure, lightMaterial, lightSampleCount, lightPosition, lightWidth, directionToSun, sunIrradiance | 196 |
| `PushConstants` | frame*, meshes*, materials*, extent | 32 (limit: 128) |
| constants | `kSlotAccumulation = 0` (HDR output image), `kSlotDisplay = 1` | |
 
> ⚠️ **A C++/Slang mismatch fails silently:** you get garbage colors, not an error. **Whenever you change `GpuStructs.h`:** (1) update the C++ code that fills the struct, (2) update the `static_assert` in `src/renderer/GpuStructsCheck.cpp`, (3) tell the team, since this file is shared by everyone.
 
### 7.4 The bindless descriptor set
**One** descriptor set (set 0) is bound once per frame and used by **every** pipeline. Its C++ side is `vk/Bindless.cpp`, and its Slang side is `shaders/common/bindings.slangh`:
 
| Binding | Slang | Contents |
|---|---|---|
| 0 | `Sampler2D gTextures[]` | Material textures (4096 slots, **reserved, not used yet**) |
| 1 | `RWTexture2D<float4> gImages[]` | Storage images, by slot constant: `gImages[kSlotAccumulation]`, `gImages[kSlotDisplay]` |
| 2 | `RaytracingAccelerationStructure gTlas` | The scene TLAS |
 
There is **one pipeline layout** for everything: set 0 + `PushConstants` (all shader stages), so pipelines never need their own layouts. The TLAS binding may only be updated while no frame is in flight; the renderer does this right after `sync()`.
 
---
 
## 8. Shaders
 
### 8.1 Files
| File | Contains |
|---|---|
| `shared/GpuStructs.h` | Shared structs (§7.3). Included by `bindings.slangh` |
| `common/bindings.slangh` | `gTextures`, `gImages`, `gTlas`, `pc` + helpers: **`fetchSurface()`** (hit position, normal, uv, material from a hit), **`primaryRay()`** (camera ray for a pixel), **`makeRay()`** |
| `integrators/pathtrace_mc.slang` | Ray tracing entry points: `rayGen`, `miss`, `shadowMiss`, `closestHit` |
| `passes/tonemap.slang` | Compute entry point `tonemap`: exposure → ACES → sRGB |
 
Every shader **entry-point file** starts with `#include "common/bindings.slangh"`.
 
### 8.2 How shaders are built
- **At build time** by `cmake/Shaders.cmake`, using the SDK's `slangc`. Shader errors appear in Visual Studio's **Output window** (look for `error[E…]` with a file and line).
- **Output:** `build/<preset>/bin/shaders/<same path>/<name>.spv`. Debug builds load them straight from there (`PT_SHADER_DIR`).
- **Flags:** `-target spirv -profile spirv_1_6 -matrix-layout-column-major` (matches GLM), `-fvk-use-scalar-layout` (matches C++ structs), `-fvk-use-entrypoint-name` (keeps names like `rayGen`), `-g2` (debug info for RenderDoc/Nsight).
- **Only entry-point files** are listed in `shaders/CMakeLists.txt`. Included files (`.slangh`, `.h`, modules) are tracked automatically, and changing one rebuilds all shaders.
- **After editing a shader**, rebuild and restart the app (there's no hot-reload yet).
### 8.3 How a ray finds its shader (SBT)
The shader binding table is built in `vk/Pipeline.cpp` in this group order: **`[rayGen] [miss, shadowMiss] [closestHit]`**.
```slang
TraceRay(gTlas, RAY_FLAG_NONE, 0xFF, /*sbtOffset*/0, /*sbtStride*/0, /*missIndex*/0, ray, payload);
//                                    └── always hit group 0 → closestHit       └── 0 → miss, 1 → shadowMiss
```
- `miss` / `closestHit` use `HitPayload`. `shadowMiss` uses `ShadowPayload` (for shadow rays: trace with `missIndex = 1` and `RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER`).
- Materials are **data** (`pc.materials[materialIndex]`), not different shaders, so one hit group is enough.
- `maxPipelineRayRecursionDepth = 1`: **don't call `TraceRay` from a hit shader.** Loop in `rayGen` instead (iterative bounces).
- Entry-point **names must match** `RayTracingPipelineDesc` in `vk/Pipeline.h`.
### 8.4 Slang pitfalls we already hit
- **Don't redefine built-in types.** `RayDesc`, `RayQuery` and similar already exist. A struct with the same name causes "expected 'RayDesc', got 'RayDesc'".
- Functions are strictly typed: `float` vs `float3` overloads don't convert automatically.
- `NonUniformResourceIndex(i)` is required when indexing `gTextures[]` with a per-thread value (later, for textures).
---
 
## 9. Daily workflow
 
### 9.1 The loop
1. `git pull`, then open the solution (re-run `cmake --preset vs2026` only after pulling CMake changes, or if `build/` was deleted).
2. Edit code in Visual Studio. **Edits go straight to the real files** in `src/` and `shaders/`; `build/` only contains generated project files.
3. **Ctrl+Shift+B** to build, **F5** to run with the debugger.
4. Check the **Output** window for `[vk]` messages.
5. Commit small, working steps (View → Git Changes, or `git` in a terminal).
### 9.2 Adding things
| To add… | Do this | Edit CMake? |
|---|---|---|
| A `.cpp` / `.h` | Create it **in the right `src/<module>/` folder**, then build. CMake picks it up automatically (click **Reload All** if Visual Studio asks). Don't let *Add → New Item* put the file in `build/` | No |
| A shader include / module | Create it under `shaders/` | No |
| A shader **entry-point** file | Create it, **add its path to `shaders/CMakeLists.txt`**, load it in `Renderer` | Yes, 1 line |
| A material parameter | `Scene.h` (`Material`) → `GpuStructs.h` (`GpuMaterial`) → copy line in `GpuScene::uploadMaterials()` → `static_assert` in `GpuStructsCheck.cpp` → set it in `SceneLoader.cpp` → read it in the shader | No |
| A per-frame value (e.g. a new setting) | `RenderSettings.h` → `FrameUniforms` in `GpuStructs.h` → `Renderer::writeFrameUniforms()` → `static_assert` → read `pc.frame[0].x` in the shader | No |
| A link between modules | `target_link_libraries(...)` in that module's `CMakeLists.txt`, respecting the layer rules (§4) | Yes |
| A compiler flag / define | `cmake/ProjectOptions.cmake` | Yes |
 
**Never change build settings in Visual Studio's *Project → Properties*** (compiler, linker, include paths). CMake regenerates the projects and overwrites them. The one exception: *Debugging → Command Arguments* is kept.
 
When things get confused, close Visual Studio, delete `build/`, and run `cmake --preset vs2026` again. Nothing is lost.
 
### 9.3 Editor setup
- **C++:** Visual Studio's IntelliSense works from the generated projects. If `shaders/shared/GpuStructs.h` shows errors, pick **`pt_renderer`** in the project drop-down at the top-left of the editor.
- **Slang:** Visual Studio's C++ IntelliSense doesn't understand `.slang` (ignore red squiggles there). For shader editing, **VS Code with the official "Slang" extension** works best. Open the project root and add `.vscode/settings.json`:
```json
{ "slang.additionalSearchPaths": ["shaders"] }
```
- **VS Code for C++** needs `compile_commands.json`, which is only generated with the **`ninja-msvc`** preset (not with the Visual Studio generator). Use the CMake Tools extension.
### 9.4 Git
- **Commit:** `src/`, `shaders/`, `cmake/`, `assets/`, all `CMakeLists.txt`, `CMakePresets.json`, `README.md`.
- **Never commit:** `build/`, `.vs/`, `*.user`, `*.spv` (`.gitignore` handles this).
- Create or initialize repositories **at the project root from a terminal**. Visual Studio's *Git → Create Git Repository* with the solution open puts the repo inside `build/`.
- Personal CMake tweaks go in `CMakeUserPresets.json`, which is ignored.
- Use feature branches + pull requests. Take extra care when merging **`GpuStructs.h`**.
---
 
## 10. Debugging
 
### 10.1 First steps
1. **Read the Output window.** Validation (`[vk] ...`) points at most Vulkan bugs directly.
2. **Vulkan Configurator** (part of the SDK) → enable:
   - **Synchronization Validation** for missing or wrong barriers.
   - **GPU-Assisted Validation** for bad device addresses and out-of-range indices in shaders.
3. **App hangs (frozen window)?** Use Visual Studio **Debug → Break All** and look at the call stack. It's usually a semaphore wait that never finishes.
4. **Wrong colors in a shader?** Temporarily write the value you're unsure about as the color (normals, `materialIndex`, hit distance). That's the quickest shader debugging there is.
### 10.2 Tools
- **RenderDoc:** launch `build\vs2026\bin\Debug\PathTracer.exe` from it, capture a frame, inspect images, buffers and the tonemap pass.
- **Nsight Graphics:** best for ray tracing. Its acceleration structure viewer shows BLAS/TLAS and instance transforms, and GPU Trace shows performance.
- Objects created with debug names (`ctx.setName`, the `debugName` constructor arguments) show up by name in both tools and in validation messages.
### 10.3 Problems we have already run into
| Symptom | Cause |
|---|---|
| Window frozen after 2 frames, validation: *"signal value (0) … must be greater"* | Timeline semaphore signaled without setting `.value` |
| Many *"X is not a member of pt::vk"* / *"undeclared identifier"* | A header uses a class by value without including its header (e.g. `vk/Resources.h`) |
| Shader build *"exited with code -1"* | The real `slangc` error is a few lines above in the Output window |
| *"expected 'RayDesc', got 'RayDesc'"* | A struct named like a Slang built-in |
| Objects mirrored or misplaced | TLAS transform: GLM is column-major, Vulkan wants row-major 3×4 (transposed in `buildTlas`) |
| Black screen with ray tracing | TLAS not bound, descriptor set not bound for the `RAY_TRACING` bind point, or SBT regions wrong |
| Garbage material/vertex data on the GPU | C++/Slang struct mismatch (§7.3) |
| Shaders "missing" assert at startup | Shader build failed, or a new entry file wasn't added to `shaders/CMakeLists.txt` |
 
---
 
## 11. Conventions
- **Namespaces:** `pt` (app/scene/renderer), `pt::vk` (Vulkan layer), `pt::gpu` (shared structs in C++), `pt::log`.
- **Includes:** always from the `src/` root: `#include "vk/Context.h"`, `#include <scene/Scene.h>`. Shared structs: `#include <shared/GpuStructs.h>`.
- **Ownership:** classes that own GPU or OS resources derive from `NonCopyable` (move-only) and free them in their destructor. Destruction order matters: anything using the device is destroyed **before** `vk::Context`. `App` declares members in creation order, so they are destroyed in reverse.
- **Errors:** `VK_CHECK(vkSomething(...))` for every Vulkan call that returns a `VkResult`; `PT_ASSERT` for things that must never happen.
- **Logging:** `log::info("Built {} things", n)`, not `printf`.
- **Naming:** `m_member`, `kConstant`, `PascalCase` types, `camelCase` functions.
- **GPU objects:** give important ones a debug name.
- **Comments:** explain *why*, especially for synchronization and anything shared with shaders.
---
 
## 12. Extending the renderer
The foundation is in place: scene on the GPU, acceleration structures, ray tracing pipeline, shared structs and the output path. New rendering work happens mostly in **shaders**:
 
| Task | Main files |
|---|---|
| Bounce loop (reflection, refraction) | `rayGen` in `pathtrace_mc.slang` (iterate with `maxBounces`, origin = hit position + small offset along the normal) |
| Materials | `GpuMaterial` already has `baseColor, roughness, emission, metallic, ior, transmission`. Read them via `pc.materials[p.materialIndex]` |
| Shadow rays | `TraceRay(..., missIndex = 1, ...)` with a `ShadowPayload` (§8.3) |
| New per-frame parameters | §9.2 "A per-frame value" |
| Additional techniques (Monte Carlo, ReSTIR, spectral) | New shader files + a few C++ additions (buffers, settings). Follow the §9.2 checklists, and coordinate changes to `GpuStructs.h` with the team |
 
---
 
## 13. Vulkan glossary for newcomers
| Term | Meaning here |
|---|---|
| **Instance / physical device / device** | Connection to Vulkan / the actual GPU / our logical handle to it. All in `vk::Context` |
| **Queue** | Where command buffers are submitted. We use one queue for everything |
| **Command buffer** | A recorded list of GPU commands; nothing runs until it's **submitted** |
| **Swapchain** | The window's images; we draw into one, then **present** it |
| **Semaphore (binary / timeline)** | GPU-side signal. Binary: acquire → submit → present. Timeline: a counter the CPU waits on to reuse frame slots |
| **Barrier / image layout** | Tells the GPU "finish writing X before reading it" and changes how an image is stored. `Image2D::transition` handles it |
| **Descriptor set** | A table of resources shaders can access. We have **one** (bindless) |
| **Push constants** | A tiny (≤ 128 B) block of data sent with each command. Ours holds GPU pointers + image size |
| **Buffer device address (BDA)** | A 64-bit GPU pointer to a buffer; shaders use it like a C pointer |
| **VMA** | Library that handles GPU memory allocation for us |
| **volk** | Loads Vulkan function pointers (faster than the default loader path) |
| **SPIR-V** | The compiled shader format Vulkan consumes (`.spv`) |
| **BLAS / TLAS** | Bottom-level acceleration structure (one mesh's triangles) / top-level (instances of BLASes with transforms). Rays are traced against the TLAS |
| **Ray tracing pipeline** | The set of ray tracing shaders (raygen, miss, closest hit) as one pipeline |
| **SBT (shader binding table)** | GPU buffer that tells the hardware which shader to run on miss/hit |
| **Ray query** | Inline ray tracing from any shader (e.g. compute) without hit/miss shaders or an SBT |
| **Payload** | The struct a ray carries between `TraceRay` and its hit/miss shader |
 
---
 
## 14. Further reading
- Nathan Reed, [Hash Functions for GPU Rendering](https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/) (PCG generator used by `nextRandom()` for light sampling)
- Vulkan specification and guide: <https://docs.vulkan.org>
- Vulkan SDK (downloads, release notes, Vulkan Configurator): <https://vulkan.lunarg.com>
- Slang language and documentation: <https://shader-slang.org>
- Slang VS Code extension: <https://github.com/shader-slang/slang-vscode-extension>
- *Physically Based Rendering* (Pharr, Jakob, Humphreys), the standard reference for path tracing; free online at pbr-book.org