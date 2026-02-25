# FragCoord Inspector for GPUpad — Implementation Plan

> **Constraint**: Zero changes to GPUpad's codebase. The feature is delivered entirely as a custom action (`extra/actions/Inspector/`) using JS scripting, QML UI, and GLSL shaders.

## Progress

| Stage | Status | Notes |
|-------|--------|-------|
| 1 — Scaffolding & expression render | ✅ **DONE** | `script.js`, `ui.qml`, `attributeless.vs` — expression renders to RGBA32F FBO |
| 2 — Shader rewrite engine | ✅ **DONE** | Validation, type inference, vec4 coercion, multi-entry rewrite (main + mainImage), range hints |
| 3 — Histogram compute pass | ✅ **DONE** | `histogram.comp` SSBO, ClearBuffer, atomicMin/Max with CAS sentinels; `script.js` pipeline wiring |
| 4 — Composite display shader | 🔄 **IN PROGRESS** | |
| 5 — QML inspector panel | ⬜ pending | |
| 6 — Per-pixel inspection | ⬜ pending | |
| 7 — Multi-shader support | ⬜ pending | |
| 8 — Polish & docs | ⬜ pending | |

---

## Architectural Summary

The inspector is a **fully GPU-resident** pipeline injected into the user's session via scripting. No CPU readback is needed — histogram computation and visualization both happen on the GPU. The QML panel provides controls; the viewport displays the mapped result with a shader-rendered histogram overlay.

```
extra/actions/Inspector/
├── script.js           # Action entry point, session manipulation, shader rewriting
├── ui.qml              # Control panel (expression input, mapping, channels)
├── attributeless.vs    # Shared fullscreen-quad vertex shader
├── inspector.fs        # Expression output shader (template, rewritten per-expression)
├── histogram.comp      # Compute shader: reads float FBO → builds histogram in SSBO
├── composite.fs        # Final display: mapped view + histogram overlay
└── CMakeLists.txt      # (only if C++ plugin is added later for advanced features)
```

---

## Stage 1 — Action Scaffolding & Manual Expression Rendering

**Goal**: Get the basic custom action working: a QML panel where you type a GLSL expression, and the inspector renders that expression's value as color in a dedicated texture.

### What to build

1. **`script.js`** — Action manifest + `Script` class that:
   - Opens `ui.qml` as a dockable panel
   - On expression change: locates the user's fragment shader, reads its source, rewrites `main()` to output the expression as `vec4`, writes the rewritten source to a cloned shader item
   - Creates inspector session items: `Texture` (RGBA32F), `Target`, cloned `Program` (with rewritten fragment shader), `Call` (Draw)
   - Cleans up inspector items when disabled

2. **`ui.qml`** — Minimal panel:
   - Text field for GLSL expression (e.g. `vTexCoords.x`, `sin(uTime)`)
   - Enable/disable toggle
   - Status label (active expression / error)

3. **`attributeless.vs`** — Fullscreen triangle-strip vertex shader (reuse from existing samples)

4. **`inspector.fs`** — Template fragment shader:
   - Includes same uniforms as the user's shader
   - Contains the user's shader code up to `main()`
   - Replaces `main()` body with `fragColor = vec4(EXPRESSION);`

### Key API usage

```js
// Read user's shader source
const shaderItem = app.session.findItem(item => item.shaderType === 'Fragment')
const source = app.readTextFile(shaderItem.fileName)

// Create inspector texture
const inspTex = app.session.insertItem(inspectorGroup, {
  type: 'Texture', name: 'InspectorFBO',
  format: 'RGBA32F', width: 'target.viewportSize[0]', height: 'target.viewportSize[1]'
})

// Write rewritten shader
const rewritten = rewriteShader(source, expression)
app.session.setShaderSource(inspShader, rewritten)
```

### Deliverable

A working action that renders `vec4(expression, expression, expression, 1.0)` to a float texture and displays it in the viewport via `openEditor("InspectorFBO")`.

### Validation

- Select a Shadertoy-style session
- Type `gl_FragCoord.x / iResolution.x` → viewport shows horizontal gradient
- Type `sin(iTime)` → viewport pulses gray

---

## Stage 2 — Shader Rewrite Engine

**Goal**: Robust GLSL rewriting that handles real-world shaders — preserving declarations, uniforms, and helper functions while injecting the inspector expression.

### What to build

1. **Expression validation** (in `script.js`):
   - Reject GLSL keywords (`void`, `return`, `if`, `for`, etc.)
   - Check balanced parentheses/brackets
   - Accept: identifiers, swizzles, function calls, arithmetic, built-in variables

2. **Type inference** (lightweight, in JS):
   - Map common types: `gl_FragCoord` → `vec4`, `gl_FragCoord.xy` → `vec2`
   - Parse `uniform <type> <name>` declarations from shader source
   - Parse local variable declarations: `float x = ...` → `x` is `float`
   - Parse function return types: `vec3 myFunc(...)` → `myFunc(...)` is `vec3`

3. **vec4 coercion** (in JS, mirroring FragCoord's `aA()`):
   ```
   float → vec4(v, v, v, 1.0)
   vec2  → vec4(v, 0.0, 1.0)
   vec3  → vec4(v, 1.0)
   vec4  → vec4(v)
   mat2  → vec4(m[0], m[1])
   ```

4. **Shader assembly** (in `script.js`):
   - Extract everything before `void main()` from the user's shader
   - Append inspector `main()` that computes and outputs the coerced expression
   - Preserve `#version`, `#extension`, precision qualifiers
   - Handle both `out vec4 oColor;` and `layout(location=0) out vec4 fragColor;` patterns

5. **Range hint parsing**:
   - Scan shader source for `// [min, max]` near the expression variable
   - Use as initial min/max for mapping

### Deliverable

The rewrite engine correctly handles the Shadertoy, Sliders, Cube, and Printf sample shaders.

### Validation

- Rewrite Sliders `image.fs` with expression `r` → outputs red channel as grayscale
- Rewrite with expression `uv` (vec2) → outputs UV as RG
- Rewrite with `sin(uv.x * 10.0)` → outputs sine pattern
- Keywords like `return` or `void` are rejected

---

## Stage 3 — Histogram Compute Pass

**Goal**: A compute shader that reads the inspector float texture and produces per-channel histogram bins in a buffer (SSBO), entirely on the GPU.

### What to build

1. **`histogram.comp`** — Compute shader:
   ```glsl
   #version 430
   layout(local_size_x = 16, local_size_y = 16) in;
   
   layout(binding = 0, rgba32f) readonly uniform image2D uInspectorTex;
   layout(std430, binding = 1) buffer HistogramBuffer {
       uint binsR[128];
       uint binsG[128];
       uint binsB[128];
       uint binsA[128];
       float dataMin;
       float dataMax;
       float autoMin;
       float autoMax;
       uint totalPixels;
   };
   
   // Two-pass approach:
   // Pass A: Clear bins + find min/max (atomicMin/Max on uint-encoded floats)
   // Pass B: Bin pixels + compute quantiles
   ```
   
   Practical simplification for PoC: **single-pass** using `atomicAdd` on bin counters and `atomicMin`/`atomicMax` on float-as-uint for range detection.

2. **Session items** (created by `script.js`):
   - `Buffer` with histogram block (128×4 uint bins + stats floats)
   - `Program` with `histogram.comp`
   - `Binding` for inspector texture (Image) and histogram buffer (BufferBlock)
   - `Call` with `callType: "Compute"`, workgroups = ceil(width/16) × ceil(height/16)
   - `Call` with `callType: "ClearBuffer"` before histogram compute (reset bins to zero)

3. **Auto-range computation**:
   - After binning, a second small compute dispatch (1 workgroup) scans bins to find 1%/99% percentile boundaries
   - Writes `autoMin`/`autoMax` to the stats section of the SSBO

### Technical notes

- `atomicAdd` on `uint` bins is straightforward
- For min/max: encode floats as uints via `floatBitsToUint()` and use `atomicMin`/`atomicMax` (works correctly for positive floats; for negative values, flip bits)
- Buffer must be cleared each frame before histogram dispatch

### Deliverable

Histogram SSBO is populated each frame with correct bin counts and min/max range.

### Validation

- Render a known gradient (0→1 linear) → bins should be roughly uniform
- Render a constant color → single bin spike
- Render `sin()` pattern → expected distribution shape

---

## Stage 4 — Composite Display Shader

**Goal**: A fragment shader that renders the mapped inspector view with a histogram overlay in the viewport.

### What to build

1. **`composite.fs`** — Fragment shader that:

   **Mapped view** (full viewport):
   - Samples the inspector float texture
   - Applies selected mapping (linear/sigmoid/log) with current min/max
   - Optionally renders out-of-range checkerboard (magenta below, cyan above)
   - In compare mode: left half shows original shader output, right half shows mapped
   
   **Histogram overlay** (bottom strip of viewport):
   - Reads histogram SSBO bin counts
   - Renders colored bars (R/G/B/A channels, toggleable via uniform ivec4)
   - Draws mapping curve (white line)
   - Draws min/max indicator lines (orange vertical lines)
   
   **Per-pixel tooltip assist**:
   - When mouse is over the mapped view, use `printf()` to output the raw float value at `uMouseFragCoord`

2. **Uniforms controlled from QML** (created as Bindings):

   | Uniform | Type | Source |
   |---------|------|--------|
   | `uMappingMode` | `int` | 0=linear, 1=sigmoid, 2=log |
   | `uMappingRange` | `vec2` | (min, max) from QML sliders or auto |
   | `uChannelMask` | `ivec4` | which channels to show in histogram |
   | `uHighlightOOR` | `int` | out-of-range checkerboard enable |
   | `uCompareMode` | `int` | 0=off, 1=split view |
   | `uCompareSplit` | `float` | split position (0..1) |
   | `uMouseFragCoord` | `vec2` | `app.mouse.fragCoord` expression |
   | `uHistogramHeight` | `float` | fraction of viewport for histogram strip |

3. **Session items**:
   - `Program` with `attributeless.vs` + `composite.fs`
   - `Binding`s for all uniforms above
   - `Binding` (Sampler) for InspectorFBO texture
   - `Binding` (BufferBlock) for histogram SSBO
   - `Call` (Draw) rendering to the display target
   - Optionally: `Binding` (Sampler) for original shader output (for compare mode)

### Deliverable

Viewport shows the mapped inspector view with a functional histogram overlay strip at the bottom.

### Validation

- Linear mapping of a gradient → smooth grayscale + flat histogram
- Switch to sigmoid → visible S-curve compression
- Enable highlight → checkerboard on out-of-range pixels
- Toggle R/G/B channels → histogram bars appear/disappear

---

## Stage 5 — QML Inspector Panel

**Goal**: Full control panel that drives the inspector pipeline via uniform bindings.

### What to build

1. **`ui.qml`** — Dockable panel with sections:

   **Expression input**:
   - Text field for GLSL expression
   - "Apply" button (or auto-apply on Enter)
   - Status label: current expression + inferred type, or error message

   **Mapping controls**:
   - Mode selector: `Linear` / `Sigmoid` / `Log` (radio buttons or segmented control)
   - Min/Max range: dual spinboxes (editable, or driven by auto-range)
   - "Auto Range" button: resets min/max to histogram's 1%–99% quantiles
   - "Highlight Out-of-Range" checkbox

   **Channel toggles**:
   - R / G / B / A checkboxes (only enabled channels show in histogram)

   **Compare mode**:
   - "Compare" checkbox: enables side-by-side split
   - Split position slider (0..1)

   **Inspector toggle**:
   - Enable/disable the entire inspector pipeline
   - When disabled: remove all inspector session items, restore original shader display

2. **QML ↔ JS ↔ Session binding**:
   ```
   QML slider/checkbox → script.js callback → modify Binding.values → GPU uniform updated
   ```
   
   Example flow:
   ```qml
   ComboBox {
     model: ["Linear", "Sigmoid", "Log"]
     onCurrentIndexChanged: {
       app.session.findItem("uMappingMode").values = [currentIndex]
     }
   }
   ```

3. **Timer-based updates** (for auto-range animation):
   - QML `Timer` polls auto-range values and smoothly interpolates min/max

### Deliverable

Full interactive control panel that drives all inspector parameters in real-time.

### Validation

- Change mapping mode → viewport updates immediately
- Adjust min/max → histogram and view respond
- Toggle channels → histogram bars change
- Enable compare → split view appears

---

## Stage 6 — Per-Pixel Value Inspection

**Goal**: Show the raw inspector value under the mouse cursor, leveraging GPUpad's built-in printf.

### What to build

1. **Printf integration in `inspector.fs`**:
   ```glsl
   #if defined(GPUPAD)
   uniform vec2 uMouseFragCoord;
   // ... inside main():
   if (gl_FragCoord.xy == uMouseFragCoord) {
       vec4 val = INSPECTOR_EXPRESSION;
       printf("Inspector [%s]: %.6f, %.6f, %.6f, %.6f",
              "EXPR_NAME", val.x, val.y, val.z, val.w);
   }
   #endif
   ```

2. **Binding**: `uMouseFragCoord` with expression `app.mouse.fragCoord`

3. **QML status display**:
   - Show the expression name and inferred type
   - Show last printf output (if we can read Message window — otherwise, user checks Messages)

4. **Cursor crosshair** (in `composite.fs`):
   - Thin crosshair lines at mouse position for visual feedback
   - Color swatch: small square at cursor showing the raw (unmapped) color

### Limitations (without codebase changes)

- Printf outputs to the Message window, not as an overlay tooltip
- No way to programmatically read the Message window from JS
- For a true tooltip, a codebase addition would be needed (future enhancement)

### Deliverable

Mouse hover over the viewport triggers printf output with raw float values; crosshair rendered in the composite shader.

### Validation

- Hover over gradient → Message window shows increasing float values
- Hover over a specific color → printf shows correct R/G/B/A
- Crosshair follows mouse position

---

## Stage 7 — Session Integration & Multi-Shader Support

**Goal**: Make the inspector work robustly with different session configurations — not just single-shader Shadertoy sessions but multi-pass, multi-program setups.

### What to build

1. **Target program selection** (in QML):
   - Dropdown listing all `Program` items in the session
   - Dropdown listing all `Call` items to inspect
   - When user selects a call, the inspector rewrites that call's fragment shader

2. **Pass texture forwarding**:
   - Detect what textures/samplers the target shader uses
   - Create matching `Binding` items in the inspector pass so the rewritten shader has access to all inputs

3. **Uniform forwarding**:
   - Clone all `Binding` items that affect the target call
   - Ensure the inspector pass has identical uniform state

4. **Multi-pass awareness**:
   - Inspector expression render should happen at the same point in the session as the target call
   - Use `insertItemAfter(targetCall, inspectorCall)` for correct ordering

5. **Session cleanup**:
   - All inspector items in a dedicated `Group` (named `__Inspector__`)
   - Toggle on/off simply checks/unchecks the group
   - Full removal when action is closed

6. **Error handling**:
   - If the rewritten shader fails to compile, show error in QML panel
   - Fall back to displaying original shader output
   - Use `processShader()` to validate before rendering

### Deliverable

Inspector works on any call in a multi-pass session, with correct texture and uniform forwarding.

### Validation

- Inspect a specific pass in the Particles (OIT) session
- Inspect the compute output in the Compute (Game of Life) session
- Switch between programs → inspector updates correctly
- Invalid expression → error shown, graceful fallback

---

## Stage 8 — Polish, Presets & Documentation

**Goal**: Finalize the action for daily use — add convenience features, presets, and documentation.

### What to build

1. **Expression presets**:
   - Quick-select buttons for common inspections:
     - `gl_FragCoord.xy / iResolution.xy` (UV)
     - `length(normal)` (normal magnitude)
     - `depth` (depth buffer value)
     - `abs(expression)` wrapper
   - "History" dropdown of recently used expressions

2. **Range animation**:
   - Smooth interpolation when auto-range updates (ease-in-out)
   - Toggle in QML panel

3. **Histogram appearance**:
   - Configurable histogram height (as fraction of viewport)
   - Logarithmic bin display (for highly skewed distributions)
   - Optional: cumulative distribution overlay

4. **Keyboard shortcuts**:
   - `Ctrl+I` to toggle inspector
   - `Ctrl+Shift+I` to apply expression from clipboard

5. **Documentation**:
   - `README.md` in the action directory explaining usage
   - Example session (`.gpjs`) demonstrating the inspector with the Sliders sample
   - Screenshots for the action description

6. **Performance**:
   - Only run histogram compute when inspector is active
   - Throttle histogram updates (every N frames) for complex shaders
   - Use `executeOn: "EveryEvaluation"` for inspector calls; disable when inspector is off

### Deliverable

Production-ready custom action with documentation, ready for inclusion in `extra/actions/`.

### Validation

- Full workflow: open Shadertoy session → enable inspector → type expression → adjust mapping → read values → disable
- No leftover session items after disabling
- Action appears in GPUpad's Session menu
- README explains all features

---

## Summary — Stage Dependencies

```
Stage 1 (Scaffolding)
    │
    ▼
Stage 2 (Rewrite Engine)
    │
    ├──────────────────┐
    ▼                  ▼
Stage 3 (Histogram)  Stage 5 (QML Panel)
    │                  │
    ▼                  │
Stage 4 (Composite) ◄─┘
    │
    ▼
Stage 6 (Per-Pixel)
    │
    ▼
Stage 7 (Multi-Shader)
    │
    ▼
Stage 8 (Polish)
```

Stages 1→2 are sequential (foundation). Stages 3 and 5 can be developed in parallel. Stage 4 integrates both. Stages 6–8 are sequential increments.

---

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| No GPU readback to JS | Cannot build histogram in JS/QML | Fully-GPU histogram via compute shader + visualization shader |
| No editor selection hook | Cannot auto-detect expression | Manual expression input in QML; sufficient for PoC |
| Shader rewrite fragility | Complex shaders may break | Start with Shadertoy-style (single main), expand incrementally |
| Printf-only pixel inspection | No rich tooltip overlay | Acceptable for PoC; document as known limitation |
| SSBO access from fragment shader | Requires GLSL 430+ | GPUpad supports OpenGL 4.3+; should work on modern GPUs |
| Inspector items pollute session | User confusion | Dedicated `__Inspector__` group; auto-cleanup on disable |

---

**End of implementation plan.**
