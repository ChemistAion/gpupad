# 🔬 FragCoord Inspector

A GPU-resident shader variable visualizer for GPUpad. Inspect any GLSL expression
as a colour-mapped overlay with a live histogram — no CPU readback, no code changes
to GPUpad itself.

## Quick start

1. Open any `.gpjs` session that contains at least one Draw call with a fragment shader.
2. **Session → 🔬 Inspector…** to open the panel.
3. Type a GLSL expression (e.g. `gl_FragCoord.xy / iResolution.xy`) and press **Apply**.
4. The editor switches to a colour-mapped view of that expression with a histogram overlay.

## Features

| Feature | Description |
|---------|-------------|
| **Expression input** | Any GLSL expression valid in the target fragment shader scope |
| **Presets** | One-click buttons for UV, Depth, Normal length, abs(…) wrapper |
| **History** | Dropdown of the 20 most recently applied expressions |
| **Mapping modes** | Linear · Sigmoid · Logarithmic, with adjustable min/max range |
| **Auto-range** | `#pragma inspector_range(min, max)` hints in shader source are auto-detected |
| **Histogram** | Per-channel histogram (R/G/B/A toggles), adjustable height |
| **Out-of-range** | Checkerboard highlight for values outside the mapping range |
| **Per-pixel printf** | Hover the mouse to read the exact vec4 value under the cursor (crosshair overlay) |
| **Multi-shader** | Target Call selector to choose which Draw call to inspect |
| **Binding forwarding** | Textures, samplers, uniforms, and buffers from the target call's scope are auto-forwarded |

## How it works

```
Expression ──► Shader rewrite ──► RGBA32F FBO ──► Histogram compute ──► Composite display
                                       │                                      │
                                       └── printf under cursor ◄──────────────┘
```

1. The target fragment shader's `main()` / `mainImage()` is rewritten to output the
   expression as an RGBA32F value.
2. A compute shader bins the FBO into a 256-bin SSBO histogram.
3. A composite fragment shader renders the colour-mapped result with the histogram
   overlay, out-of-range highlighting, and a crosshair at the mouse position.

Everything runs on the GPU. The only CPU-side work is the JS scripting that builds
the session pipeline.

## Panel controls

- **Expression** — free-form GLSL expression, or use a preset button.
- **Target Call** — which Draw call to inspect (auto-populated, refreshable).
- **Mapping Mode / Range** — how values map to colour. Use the spinboxes or let
  `#pragma inspector_range` set the range automatically.
- **Histogram Channels** — toggle R, G, B, A visibility in the histogram.
- **Histogram Height** — fraction of the viewport used by the histogram overlay.
- **Highlight out-of-range** — shows a checkerboard where values fall outside [min, max].
- **Disable Inspector** — removes all inspector items from the session.

## Shader compatibility

- Standard GLSL `void main()` entry points.
- Shadertoy-style `void mainImage(out vec4 fragColor, in vec2 fragCoord)`.
- Requires `#version 430` or higher (for compute shaders and SSBOs).
- The `#if defined(GPUPAD)` guard protects printf injection so shaders stay portable.

## Files

| File | Purpose |
|------|---------|
| `script.js` | Action logic — validation, shader rewrite, session pipeline, QML helpers |
| `ui.qml` | Inspector panel UI |
| `composite.fs` | Composite display shader (mapping + histogram + crosshair) |
| `histogram.comp` | Compute shader for histogram binning |
| `attributeless.vs` | Fullscreen-quad vertex shader |

## Limitations

- Expression must be valid in the fragment shader's scope (no cross-stage inspection).
- History is per-session (not persisted across GPUpad restarts).
- Complex multi-pass shaders may need manual Target Call selection.
- Histogram uses 256 fixed bins; extreme dynamic range may lose detail.
