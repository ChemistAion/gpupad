# FragCoord Inspector → GPUpad — Deep-Dive Research Report

## 1. What the FragCoord Inspector Actually Does

The "Inspector" in XorDev's [fragcoord.xyz](https://fragcoord.xyz/) is a **real-time shader variable visualizer**. The user selects a GLSL expression in the editor, and the tool:

1. **Rewrites** the fragment shader so that it outputs the selected expression (coerced to `vec4`) instead of the original `fragColor`.
2. **Renders** that rewritten shader to a **float FBO** (RGBA32F when `EXT_color_buffer_float` is available, RGBA8 fallback).
3. **Downsamples** the float FBO to 128×128 and reads all pixels back to the CPU.
4. **Computes a histogram** (128 bins per channel) and derives auto-range (1st–99th percentile).
5. **Generates a mapping shader** (`_inspMap`) that remaps the expression values through linear / sigmoid / log tone-mapping with optional out-of-range checkerboard highlighting.
6. **Displays** the mapped result in the viewport and draws the histogram + mapping curve in a 2-D canvas overlay.
7. **Reads single pixels** under the cursor for a per-pixel tooltip with raw values + color swatch.

Everything is WebGL2 / GLSL ES 3.00. The UI is React; the editor is Monaco.

### Core Pipeline (verbatim from bundle analysis)

```
User selects expression in Monaco
        │
        ▼
Selection parsing (jy, fJ, D$e, L$e)
  - reject keywords/incomplete expressions
  - extract RHS of return/assignment
  - detect #define scalars, vec literals, // [min,max] range hints
        │
        ▼
Type inference (hN, ZP, Vz, ND)
  - infer float/int/vecN/matN from declarations, builtins, function sigs
  - coerce to vec4 via aA()
        │
        ▼
Shader rewrite (kqe → kJ for raw, Lqe → DJ for mapped)
  - inject `fragColor = vec4(expr)` (raw) or `fragColor = _inspMap(vec4(expr))` (mapped)
  - strip existing fragColor assignments, normalize loops
        │
        ▼
Inspector render (b3 → float FBO)
  - sets standard uniforms: u_resolution, u_time, u_frame, u_mouse, u_date
  - binds pass textures (u_passN) and main texture (u_main)
        │
        ▼
Downsample (bJ → 128×128 FBO) + readPixels (v3)
        │
        ▼
Histogram computation (Fqe)
  - 128 bins per channel (R/G/B/A)
  - auto-range: 1%–99% quantile
        │
        ▼
Canvas rendering (2D context)
  - histogram bars (per-channel, toggleable)
  - mapping curve overlay (white line)
  - min/max handle lines (orange)
  - median marker
        │
        ▼
Hover readback (ub → single pixel) → tooltip
```

### Mapping Modes

| Mode     | Formula (normalized `t = (v - min) / range`) | Behavior |
|----------|----------------------------------------------|----------|
| `linear` | `clamp(t, 0, 1)` | Direct proportion |
| `sigmoid`| `1 / (1 + exp(-8 * (2t - 1)))` | S-curve, soft clip |
| `log`    | `log2(1 + t*255) / log2(256)` | Expand darks, compress brights |

All modes optionally overlay a **magenta/cyan checkerboard** for values below-min / above-max.

### Key Dependencies (from the WebGL implementation)

- Float FBO (RGBA32F) with `readPixels(GL_FLOAT)` for accurate histogram
- Synchronous `readPixels` (acceptable in WebGL; problematic in desktop GL without PBOs)
- Scissor rendering for compare mode (left = original, right = mapped)
- `gl_FragCoord`-based patterns for checkerboard and function argument defaults

---

## 2. GPUpad GLSL Samples — Capability Atlas

### Session Architecture (`.gpjs`)

A GPUpad session is a flat JSON tree of typed items:

| Item Type | Purpose | Key Properties |
|-----------|---------|----------------|
| `Texture` | 2D/3D/Cube textures, render targets | `format`, `width`, `height`, `target` |
| `Target` | Render target with attachments | `cullMode`, `frontFace`, blend/stencil/depth per-attachment |
| `Buffer` | Structured binary data | `Block` children with `Field` rows |
| `Program` | Shader collection | `Shader` children (Vertex/Fragment/Compute/...) |
| `Binding` | Uniform/Sampler/Image/Buffer/SSBO | `bindingType`, `editor`, `values` |
| `Call` | Draw/Compute/Clear/Swap dispatch | `callType`, `programId`, `targetId`, workgroups |
| `Stream` | Vertex attributes | `Attribute` children → Buffer fields |
| `Script` | JavaScript execution | `executeOn`: Every/Reset/Manual |
| `Group` | Scoping/iteration container | `inlineScope`, `iterations` |

### Feature Inventory from Samples

**Compute shaders**: Used in 11 samples. Pattern: `Program` with `.comp` shader → `Call` with `callType: "Compute"` + `workGroupsX/Y/Z`. Can read/write Image bindings and Buffers (SSBO).

**Multi-pass rendering**: Stencil Buffer (mask→fill), Particles/OIT (clear→draw→composite), Compute (compute→swap→compute), Bitonic sort (dynamic N-pass). Pattern: multiple `Call` items evaluated sequentially.

**Float textures**: `RGBA32F` format used in Volume and ray tracing samples. Fully supported as render targets and Image bindings.

**Ping-pong / SwapTextures**: `callType: "SwapTextures"` swaps two textures between passes. Used in Compute (Game of Life).

**Image bindings (SSBO/Image)**: Compute shaders use `bindingType: "Image"` with specific formats (`rgba8ui`, `Internal`, etc.) and `bindingType: "Buffer"` for SSBOs.

**Printf debugging**: The Printf sample uses `#if defined(GPUPAD)` + `printf()` with `uMouseFragCoord` to output per-pixel values to the Message window. This is a built-in GPUpad feature.

**Dynamic viewport binding**: `target.viewportSize[0/1]` expressions in bindings for resolution-dependent rendering.

**Script-driven session modification**: Bitonic Sort dynamically inserts/removes compute calls each frame. Custom Actions sample uses `app.callAction()` to invoke GenerateMesh/ImportOBJ.

### Samples Most Relevant to Inspector

| Sample | Relevance |
|--------|-----------|
| **Printf** | Per-pixel value inspection via mouse coord; `#if defined(GPUPAD)` guard |
| **Compute** | Ping-pong texture pattern; compute shader writing to images |
| **Bitonic Sort** | Dynamic multi-pass session manipulation from JS |
| **Shadertoy / Sliders** | Fullscreen fragment shader + uniform control + `openEditor` for viewport |
| **Atomic Counters** | SSBO read/write; atomic operations in shaders |
| **Custom Actions** | `app.callAction()` from scripts; action composition |

---

## 3. GPUpad Custom Actions — Extension Mechanism

### Action Discovery & Layout

Actions live in `extra/actions/`. Each is a directory (C++ + JS + QML) or a standalone `.js` file.

**Invocation**: Menu item (auto-registered from `manifest.name`) or programmatic via `app.callAction(id, args)`.

### Pure JavaScript Actions

**Pattern**: A `script.js` with a `manifest` object and a class that orchestrates via the GPUpad scripting API.

```js
const manifest = { name: "&Action Name..." }
class Script {
  constructor() { /* load libraries, init state */ }
  initializeUi(ui) { /* wire QML controls */ }
  // ... action logic
}
this.script = new Script()
app.openEditor("ui.qml", manifest.name)  // opens QML panel
```

**Key APIs available**:

| Category | Functions |
|----------|-----------|
| Session query | `findItem()`, `findItems()`, `getParentItem()` |
| Session CRUD | `insertItem()`, `insertItemAfter()`, `deleteItem()`, `clearItems()`, `replaceItems()` |
| Data | `setBlockData()`, `setBufferData()`, `setTextureData()`, `setShaderSource()`, `setScriptSource()` |
| Shader | `processShader(shader, type)` — compile to SPIR-V binary/AST |
| Handles | `getBufferHandle()`, `getTextureHandle()` — bindless GPU handles |
| Files | `readTextFile()`, `writeTextFile()`, `writeBinaryFile()`, `enumerateFiles()` |
| UI | `openEditor(filename, title)`, `openFileDialog(pattern)` |
| External | `loadLibrary(filename)` — load C++ DLL, `callAction(id, args)` |
| Input | `app.mouse.fragCoord`, `app.mouse.button[]`, `app.keyboard.keys[]` |
| Time | `app.time`, `app.timeDelta`, `app.frameIndex`, `app.frameRate`, `app.date` |

### C++ Plugin Actions (DLLReflect)

**Pattern**: `module.cpp` using `DLLREFLECT_BEGIN/FUNC/END` macros → compiled as shared library → loaded via `app.loadLibrary("PluginName")`.

```cpp
#include "dllreflect.h"
MyType myFunction(const std::string& json) { ... }
DLLREFLECT_BEGIN()
DLLREFLECT_FUNC(myFunction)
DLLREFLECT_END()
```

**CMakeLists.txt**: `add_library(PluginName SHARED module.cpp)` + include dllreflect headers.

**Capability**: Pure CPU computation. Functions receive JSON strings and return POD types, strings, or `std::vector<T>`. They do **not** have access to the OpenGL context.

### QML UI Integration

Actions use `app.openEditor("ui.qml", title)` to open a dockable panel. QML code can:
- Call back into JS via `script.methodName()`
- Access `app.session.findItem(id)` directly to modify bindings
- Use standard Qt Quick controls (Slider, ComboBox, CheckBox, etc.)
- Use `Timer` for periodic updates

### Existing Action Examples

| Action | Type | What it does |
|--------|------|--------------|
| **GenerateMesh** | C++ + JS + QML | Procedural mesh generation (par_shapes) → inserts Buffer/Stream/Call |
| **ImportOBJ** | C++ + JS + QML | OBJ file loading (rapidobj) → inserts scene hierarchy |
| **Sliders** | JS + QML | Enumerates all Bindings → creates sliders to control their values |
| **Timer** | JS + QML | Displays app.time / frameRate in a panel |
| **Compile to SPIR-V** | JS only | Iterates shaders → `processShader()` → writes binary files |
| **Import glTF** | JS only | Parses glTF JSON → builds full scene in session |
| **Insert Orbit Camera** | JS only | Injects camera controller script + binding |
| **NodeGraph** | QML only | Standalone node-graph editor UI |

---

## 4. Critical Gap Analysis — What GPUpad Has vs. What Inspector Needs

### ✅ Available

| Need | GPUpad Capability |
|------|-------------------|
| Float FBO | `Texture` with `format: "RGBA32F"` + `Target` + `Attachment` |
| Shader rewriting | `setShaderSource(ItemIdent, newSource)` — modify shader text at runtime |
| Multi-pass rendering | Multiple `Call` items evaluated sequentially |
| Compute shaders | `callType: "Compute"` with Image/Buffer bindings |
| SSBO | `bindingType: "Buffer"` or `"BufferBlock"` with structured data |
| Dynamic session modification | `insertItem()` / `deleteItem()` / `replaceItems()` from JS |
| Uniform control from QML | Binding with `editor: "Expression"` → modify `values` from QML |
| Per-pixel printf | Built-in `printf()` with `uMouseFragCoord` |
| Viewport display | `openEditor(texture_name)` opens texture in viewport |
| QML panels | `app.openEditor("ui.qml", title)` |
| C++ plugins | DLLReflect for CPU-side computation |

### ❌ Missing (Critical)

| Need | Gap | Workaround |
|------|-----|------------|
| **GPU → CPU readback** | No `getTextureData()` or `getBlockData()` in scripting API | Keep histogram entirely on GPU: compute shader → SSBO → visualization shader |
| **Overlay rendering** | No native overlay/HUD system | Render histogram as part of the final composite pass (shader-rendered overlay) |
| **Editor selection events** | No API to hook into text editor cursor/selection | Use uniform binding expressions or manual text entry in QML for expression input |
| **Automatic expression parsing** | No access to editor's AST or cursor position | User types expression manually (or copies from editor); parse in JS |

### ⚠️ Partially Available

| Need | Status | Notes |
|------|--------|-------|
| **Per-pixel value tooltip** | Printf works but outputs to Message window, not as overlay | Acceptable for PoC; rich tooltip would need codebase changes |
| **Histogram in QML** | No readback to feed QML canvas | GPU-rendered histogram in viewport; QML only for controls |
| **Compare mode** | No scissor API in session model | Two draw calls with different viewports, or shader-based split |

---

## 5. Architectural Decision: Fully-GPU Inspector

Given the constraint that the **codebase must remain untouched** and the extension must work purely through the custom action mechanism, the architecture must be **fully GPU-resident**:

```
┌─────────────────────────────────────────────────────────┐
│  QML Control Panel (ui.qml)                             │
│  ┌──────────────┐ ┌──────────┐ ┌──────────────────────┐ │
│  │ Expression    │ │ Mapping  │ │ Channels / Highlight │ │
│  │ text input    │ │ mode     │ │ toggles              │ │
│  └──────┬───────┘ └────┬─────┘ └──────────┬───────────┘ │
│         │              │                   │             │
│         ▼              ▼                   ▼             │
│   Binding uniforms fed to inspector shaders              │
└─────────────────────────────────────────────────────────┘
           │
           ▼
┌─────────────────────── GPU Pipeline ─────────────────────┐
│                                                          │
│  Pass 1: Inspector Expression Render                     │
│  ┌─────────────────────────────────────────────────────┐ │
│  │ Rewritten fragment shader outputs selected expr     │ │
│  │ → RGBA32F float texture ("InspectorFBO")            │ │
│  └─────────────────────────────────────────────────────┘ │
│                         │                                │
│  Pass 2: Histogram Compute                               │
│  ┌─────────────────────────────────────────────────────┐ │
│  │ Compute shader reads InspectorFBO                   │ │
│  │ Builds 128 bins per channel → histogram SSBO        │ │
│  │ Computes min/max/autoRange → stats SSBO             │ │
│  └─────────────────────────────────────────────────────┘ │
│                         │                                │
│  Pass 3: Composite Display                               │
│  ┌─────────────────────────────────────────────────────┐ │
│  │ Fragment shader reads:                              │ │
│  │   - InspectorFBO (for mapped value display)         │ │
│  │   - histogram SSBO (for bar rendering)              │ │
│  │   - stats SSBO (for auto-range / curve)             │ │
│  │ Renders: mapped view + histogram overlay            │ │
│  │ Optional: compare split (original left, mapped right)│ │
│  └─────────────────────────────────────────────────────┘ │
│                                                          │
│  Printf: per-pixel value on mouse hover → Message window │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

This architecture requires:
- **No codebase changes**
- **No CPU readback** (all analysis stays on GPU)
- Standard GPUpad session items only
- A custom action with JS + QML + shader files

The trade-off is that the histogram is rendered in the viewport rather than in a separate QML canvas, but this is actually closer to how professional shader debuggers (RenderDoc, Nsight) work.

---

## 6. File-by-File Reference of FragCoord Submodule

| File | Content | Relevance |
|------|---------|-----------|
| `fragcoord-inspector-REPORT.md` | Full engineering report (this document's #1 source) | Architecture, pipeline, algorithms |
| `index-CCDNXA9l.js` | Minified bundle (React + WebGL) | Primary source for all logic |
| `index-DssYMI_M.css` | Inspector styles | UI layout reference |
| `inspector_snippet.txt` | React UI JSX for inspector panel | UI structure |
| `inspector_selection_snippet.txt` | Expression parsing / validation | Selection logic to port |
| `inspector_constants_snippet.txt` | Range hint parsing, mapping defaults | `// [min,max]` syntax |
| `inspector_deep_snippet.txt` | Type inference, shader rewrite, histogram | Core algorithms |
| `inspector_render_snippet.txt` | WebGL render pipeline | Render pass structure |
| `inspector_canvas_handlers.txt` | Mouse/touch for histogram interaction | Interaction model |
| `inspector_runtime_snippet.txt` | FBO allocation, readPixels helpers | GPU resource management |
| `inspector_histogram_snippet.txt` | Histogram computation logic | Bin building algorithm |
| `inspector_rewrite_snippet.txt` | Shader code transformation | Rewrite injection patterns |
| `inspector_state_snippet.txt` | Inspector state management | State model |
| `inspector_pipeline_snippet.txt` | Full multi-pass pipeline | Pipeline orchestration |
| `inspector_curve_snippet.txt` | Mapping curve rendering | Curve visualization |
| `inspector_onhist_snippet.txt` | Histogram request/response | Async histogram flow |
| `inspector_refs_snippet.txt` | React ref management | Internal plumbing |
| `inspector_css_snippet.txt` / `2.txt` | CSS styles | Styling reference |
| `inspector_*_markers.txt` | Code position markers | Navigation aids |
| `script.js` | Minified entry point | Application bootstrap |

---

**End of deep-dive report.**
