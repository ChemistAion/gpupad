# GPUpad — Complete Feature Encyclopedia

> **Audience**: AI coding assistants, plugin/action authors, session designers.
> **Scope**: Every user-facing feature, every session-item type, every scripting API surface, every demo session — exhaustively catalogued from source code and samples.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Session & the GPJS File Format](#2-session--the-gpjs-file-format)
3. [Session Items — Complete Reference](#3-session-items--complete-reference)
4. [Enumerations — Complete Reference](#4-enumerations--complete-reference)
5. [Evaluation Pipeline](#5-evaluation-pipeline)
6. [Scripting API](#6-scripting-api)
7. [Custom Actions](#7-custom-actions)
8. [Built-in Shader Features](#8-built-in-shader-features)
9. [Sample Sessions Catalogue](#9-sample-sessions-catalogue)
10. [Existing Custom Actions Catalogue](#10-existing-custom-actions-catalogue)

---

## 1. Overview

GPUpad is a lightweight IDE for GPU algorithm development. It supports **OpenGL**, **Vulkan**, and **Direct3D 12** renderers with GLSL, HLSL, and Slang shader languages.

**Core workflow**: Define a *session* (`.gpjs`) describing GPU resources (textures, buffers, programs, targets, bindings) and *calls* (draw, compute, ray-trace). Evaluate the session to see results. Extend with JavaScript scripts and custom actions.

**Key capabilities**:
- Fully customizable render state (blend, depth, stencil, polygon mode, logic ops)
- Compute shaders with SSBO, image load/store, atomic operations
- Hardware ray tracing (Vulkan) with acceleration structures
- Mesh and task shaders (NV extension)
- Tessellation pipeline (control + evaluation + geometry stages)
- Printf-style shader debugging with per-pixel output
- JavaScript expressions in uniform bindings (live-evaluated each frame)
- JavaScript scripting for dynamic session manipulation
- Custom actions (JS + QML + optional C++ plugins) for tool extensions
- Video file playback as texture source
- KTX/DDS support for 3D/array/cube/compressed textures

---

## 2. Session & the GPJS File Format

### 2.1 Format Basics

A `.gpjs` file is **JSON**. The top-level is either an array of items or a single item object. Each item has the structure:

```json
{
  "type": "ItemTypeName",
  "name": "Human-readable name",
  "id": 123,
  "items": [ /* child items */ ],
  /* ...type-specific properties... */
}
```

- **Enum values** are stored as their **string key names** (e.g. `"format": "RGBA8_UNorm"`, `"callType": "Draw"`).
- **Expressions** are stored as strings and evaluated as JavaScript at runtime (e.g. `"width": "1024"`, `"workGroupsX": "Math.ceil(count / 256)"`).
- **Item references** are stored as integer IDs (e.g. `"programId": 42`, `"textureId": 7`).
- **`id`** values are session-unique integers. GPUpad auto-assigns them.
- Items can be dragged to/from a text editor (serialized as JSON) and copy/pasted between instances.

### 2.2 Session-Level Properties

| Property | Type | Description |
|----------|------|-------------|
| `renderer` | `Renderer` | `OpenGL`, `Vulkan`, `Direct3D` |
| `shaderLanguage` | `ShaderLanguage` | `GLSL`, `HLSL`, `Slang`, `None` |
| `shaderCompiler` | `ShaderCompiler` | `Driver`, `glslang`, `D3DCompiler`, `DXC`, `Slang` |
| `shaderCompilerSettings` | Object | Compiler-specific key-value settings |
| `shaderPreamble` | String | Global code prefixed to every shader (e.g. `#version 460`) |
| `shaderIncludePaths` | String | Semicolon-separated include search paths |
| `flipViewport` | Bool | Flip viewport Y axis |
| `reverseCulling` | Bool | Reverse front-face winding |

### 2.3 Item Hierarchy Rules

```
Session
├── Group (nestable, creates scope unless inlineScope=true)
│   ├── Program
│   │   └── Shader (Vertex, Fragment, Compute, Mesh, Task, TessControl, TessEval, Geometry, Includable, Ray*)
│   ├── Texture
│   ├── Buffer
│   │   └── Block
│   │       └── Field
│   ├── Target
│   │   └── Attachment
│   ├── Binding
│   ├── Stream
│   │   └── Attribute
│   ├── Call
│   ├── Script
│   ├── AccelerationStructure
│   │   ├── Instance
│   │   └── Geometry
│   └── Group (recursive)
```

- **Scoping**: Groups open a new scope. Items in a scope are invisible outside (they don't appear in combo boxes). Set `inlineScope: true` to share scope with parent.
- **Evaluation order**: Calls execute top-to-bottom. Bindings apply to all subsequent calls in the same scope until shadowed.

---

## 3. Session Items — Complete Reference

### 3.1 Group

Organizational container for structuring complex sessions.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `iterations` | Expression | `"1"` | Number of times to evaluate children |
| `inlineScope` | Bool | `false` | When true, children share parent's scope |
| `dynamic` | Bool | `false` | Dynamic group flag |

### 3.2 Program

Container for shader stages. No additional properties beyond `name`.

Child items: one or more **Shader** items.

### 3.3 Shader

Individual shader source file.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `fileName` | String | — | Path to shader source file |
| `shaderType` | `ShaderType` | — | Stage type (see enum) |
| `entryPoint` | String | `""` | Entry point function name |
| `preamble` | String | `""` | Per-shader code prefix (prepended to source) |
| `includePaths` | String | `""` | Per-shader include paths |

**Shader types**: `Vertex`, `Fragment`, `Geometry`, `TessControl`, `TessEvaluation`, `Compute`, `Task`, `Mesh`, `Includable`, `RayGeneration`, `RayIntersection`, `RayAnyHit`, `RayClosestHit`, `RayMiss`, `RayCallable`

The `Includable` type marks a shader as a shared include (not compiled as a stage).

### 3.4 Texture

GPU texture resource. Can be backed by an image/video file.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `fileName` | String | `""` | Image/video file path (KTX, DDS, PNG, JPG, MP4, etc.) |
| `target` | `TextureTarget` | `Target2D` | Dimensionality |
| `format` | `TextureFormat` | `RGBA8_UNorm` | Pixel format |
| `width` | Expression | `"256"` | Width in pixels |
| `height` | Expression | `"256"` | Height in pixels |
| `depth` | Expression | `"1"` | Depth (3D textures) |
| `layers` | Expression | `"1"` | Array layer count |
| `samples` | Int | `1` | MSAA sample count |
| `flipVertically` | Bool | `false` | Flip on load |

**Texture targets**: `Target1D`, `Target1DArray`, `Target2D`, `Target2DArray`, `Target3D`, `TargetCubeMap`, `TargetCubeMapArray`

### 3.5 Buffer

Binary data container. Can be backed by a binary file.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `fileName` | String | `""` | Binary file path |

Child items: one or more **Block** items.

### 3.6 Block

A structured region within a Buffer.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `offset` | Expression | `"0"` | Byte offset within parent buffer |
| `rowCount` | Expression | `"1"` | Number of data rows |

Child items: one or more **Field** items.

### 3.7 Field

A single data column within a Block.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `dataType` | `DataType` | `Float` | Element data type |
| `count` | Int | `1` | Number of elements per row (e.g. 3 for vec3) |
| `padding` | Int | `0` | Trailing padding bytes |

### 3.8 Binding

Binds data to a program's named binding point. Affects all subsequent calls in scope until shadowed by another binding with the same name.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `bindingType` | `BindingType` | `Uniform` | Kind of binding |
| `editor` | `BindingEditor` | `Expression` | UI editor type |
| `values` | String[] | `[]` | Value expressions |
| `textureId` | ItemId | — | Referenced texture (Sampler/Image) |
| `bufferId` | ItemId | — | Referenced buffer |
| `blockId` | ItemId | — | Referenced block |
| `level` | Int | `0` | Mipmap level |
| `layer` | Int | `-1` | Array layer (-1 = all layers) |
| `minFilter` | `Filter` | `Linear` | Minification filter |
| `magFilter` | `Filter` | `Linear` | Magnification filter |
| `anisotropic` | Bool | `false` | Anisotropic filtering |
| `wrapModeX` | `WrapMode` | `Repeat` | X-axis wrap |
| `wrapModeY` | `WrapMode` | `Repeat` | Y-axis wrap |
| `wrapModeZ` | `WrapMode` | `Repeat` | Z-axis wrap |
| `borderColor` | Color | — | Border clamp color |
| `comparisonFunc` | `ComparisonFunc` | — | Depth comparison for shadow samplers |
| `subroutine` | String | `""` | Subroutine function name |
| `imageFormat` | `ImageBindingFormat` | `Internal` | Image binding format |

**Binding types**: `Uniform`, `Sampler`, `Buffer`, `BufferBlock`, `Image`, `TextureBuffer`, `Subroutine`

**Editor types**: `Expression`, `Expression2`, `Expression3`, `Expression4`, `Expression2x2`…`Expression4x4`, `Color`

**Expression bindings** are live-evaluated as JavaScript each frame. All `app.*` globals are available (e.g. `app.time`, `app.mouse.fragCoord`, `Math.sin(app.time)`).

### 3.9 Target

Render target with framebuffer configuration.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `frontFace` | `FrontFace` | `CCW` | Front-face winding |
| `cullMode` | `CullMode` | `NoCulling` | Face culling mode |
| `polygonMode` | `PolygonMode` | `Fill` | Polygon rasterization mode |
| `logicOperation` | `LogicOperation` | `NoLogicOperation` | Framebuffer logic op |
| `blendConstant` | Color | — | Blend constant color |
| `defaultWidth` | Expression | — | Default attachment width |
| `defaultHeight` | Expression | — | Default attachment height |
| `defaultLayers` | Expression | — | Default attachment layers |
| `defaultSamples` | Int | `1` | Default MSAA samples |

Child items: one or more **Attachment** items.

### 3.10 Attachment

Single color/depth/stencil attachment on a Target.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `textureId` | ItemId | — | Attached texture |
| `level` | Int | `0` | Mipmap level |
| `layer` | Int | `-1` | Array layer (-1 = all) |

**Blend state** (per-attachment):

| Property | Type | Default |
|----------|------|---------|
| `blendColorEq` | `BlendEquation` | `Add` |
| `blendAlphaEq` | `BlendEquation` | `Add` |
| `blendColorSource` | `BlendFactor` | `One` |
| `blendColorDest` | `BlendFactor` | `Zero` |
| `blendAlphaSource` | `BlendFactor` | `One` |
| `blendAlphaDest` | `BlendFactor` | `Zero` |
| `colorWriteMask` | UInt | `0xF` | RGBA write mask |

**Depth state**:

| Property | Type | Default |
|----------|------|---------|
| `depthComparisonFunc` | `ComparisonFunc` | — |
| `depthWrite` | Bool | `true` |
| `depthClamp` | Bool | `false` |
| `depthOffsetSlope` | Double | `0.0` |
| `depthOffsetConstant` | Double | `0.0` |

**Stencil state** (separate front/back):

| Property | Type | Default |
|----------|------|---------|
| `stencilFront/BackComparisonFunc` | `ComparisonFunc` | — |
| `stencilFront/BackReference` | Int | `0` |
| `stencilFront/BackReadMask` | UInt | `255` |
| `stencilFront/BackWriteMask` | UInt | `255` |
| `stencilFront/BackFailOp` | `StencilOperation` | `Keep` |
| `stencilFront/BackDepthFailOp` | `StencilOperation` | `Keep` |
| `stencilFront/BackDepthPassOp` | `StencilOperation` | `Keep` |

### 3.11 Stream

Vertex attribute stream for draw calls.

Child items: one or more **Attribute** items.

### 3.12 Attribute

Individual vertex attribute within a Stream.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `fieldId` | ItemId | — | Referenced Buffer Field |
| `normalize` | Bool | `false` | Normalize integer values to [0,1] |
| `divisor` | Int | `0` | Instance divisor (0 = per-vertex, 1+ = per-N-instances) |

### 3.13 Call

GPU execution command. All active calls evaluate top-to-bottom during session evaluation.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `callType` | `CallType` | `Draw` | Execution type |
| `executeOn` | `ExecuteOn` | `EveryEvaluation` | When to execute |
| `checked` | Bool | `true` | Enable/disable toggle |
| `programId` | ItemId | — | Program to use |
| `targetId` | ItemId | — | Render target |
| `vertexStreamId` | ItemId | — | Vertex stream |

**Draw parameters**:

| Property | Type | Description |
|----------|------|-------------|
| `primitiveType` | `PrimitiveType` | Geometry topology |
| `count` | Expression | Vertex count |
| `first` | Expression | First vertex offset |
| `instanceCount` | Expression | Instance count |
| `baseInstance` | Expression | Base instance offset |
| `patchVertices` | Expression | Tessellation patch size |

**Indexed draw**:

| Property | Type | Description |
|----------|------|-------------|
| `indexBufferBlockId` | ItemId | Index buffer block |
| `baseVertex` | Expression | Base vertex offset |

**Indirect draw**:

| Property | Type | Description |
|----------|------|-------------|
| `indirectBufferBlockId` | ItemId | Indirect command buffer |
| `drawCount` | Expression | Number of indirect draws |

**Compute**:

| Property | Type | Description |
|----------|------|-------------|
| `workGroupsX` | Expression | X work groups |
| `workGroupsY` | Expression | Y work groups |
| `workGroupsZ` | Expression | Z work groups |

**Ray tracing**:

| Property | Type | Description |
|----------|------|-------------|
| `accelerationStructureId` | ItemId | Top-level acceleration structure |

**Texture/Buffer operations**:

| Property | Type | Description |
|----------|------|-------------|
| `textureId` | ItemId | Target texture (clear/copy/swap) |
| `fromTextureId` | ItemId | Source texture (copy) |
| `clearColor` | Color | Clear color value |
| `clearDepth` | Double | Clear depth value |
| `clearStencil` | Int | Clear stencil value |
| `bufferId` | ItemId | Target buffer (clear/copy/swap) |
| `fromBufferId` | ItemId | Source buffer (copy) |

### 3.14 Script

JavaScript file executed during evaluation.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `fileName` | String | — | Script source file path |
| `executeOn` | `ExecuteOn` | `EveryEvaluation` | When to execute |

Scripts share a single JavaScript state per session. Evaluation is sequential, top-to-bottom. Group scopes do **not** isolate script state.

### 3.15 AccelerationStructure

Top-level acceleration structure for ray tracing (Vulkan).

Child items: **Instance** and/or **Geometry** items.

### 3.16 Instance

Ray tracing instance within an AccelerationStructure.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `transform` | Expression | — | 3×4 transformation matrix |

### 3.17 Geometry

Ray tracing geometry within an AccelerationStructure.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `geometryType` | `GeometryType` | `Triangles` | `Triangles` or `AxisAlignedBoundingBoxes` |
| `vertexBufferBlockId` | ItemId | — | Vertex data block |
| `indexBufferBlockId` | ItemId | — | Index data block |
| `transformBufferBlockId` | ItemId | — | Per-geometry transform block |
| `count` | Expression | — | Geometry count |
| `offset` | Expression | — | Byte offset |

---

## 4. Enumerations — Complete Reference

### 4.1 DataType
`Int8`, `Int16`, `Int32`, `Int64`, `Uint8`, `Uint16`, `Uint32`, `Uint64`, `Float`, `Double`

### 4.2 ShaderType
`Includable`, `Vertex`, `Fragment`, `Geometry`, `TessControl`, `TessEvaluation`, `Compute`, `Task`, `Mesh`, `RayGeneration`, `RayIntersection`, `RayAnyHit`, `RayClosestHit`, `RayMiss`, `RayCallable`

### 4.3 ShaderLanguage
`None`, `GLSL`, `HLSL`, `Slang`

### 4.4 ShaderCompiler
`Driver`, `glslang`, `D3DCompiler`, `DXC`, `Slang`

### 4.5 Renderer
`OpenGL`, `Vulkan`, `Direct3D`

### 4.6 CallType
| Value | Description |
|-------|-------------|
| `Draw` | Standard draw call |
| `DrawIndexed` | Indexed draw call |
| `DrawIndirect` | Indirect draw from buffer |
| `DrawIndexedIndirect` | Indexed indirect draw |
| `DrawMeshTasks` | Mesh shader dispatch |
| `Compute` | Compute shader dispatch |
| `ComputeIndirect` | Indirect compute from buffer |
| `TraceRays` | Ray tracing dispatch (Vulkan) |
| `ClearTexture` | Clear a texture to a color/depth/stencil value |
| `CopyTexture` | Copy texture contents |
| `SwapTextures` | Swap two texture contents (ping-pong) |
| `ClearBuffer` | Clear a buffer |
| `CopyBuffer` | Copy buffer contents |
| `SwapBuffers` | Swap two buffer contents |

### 4.7 ExecuteOn
| Value | When |
|-------|------|
| `ResetEvaluation` | Only on session/shader reset |
| `ManualEvaluation` | On reset or manual trigger (F6) |
| `EveryEvaluation` | Every frame (F7 auto / F8 steady) |

### 4.8 PrimitiveType
`Points`, `LineStrip`, `LineLoop`, `Lines`, `LineStripAdjacency`, `LinesAdjacency`, `TriangleStrip`, `TriangleFan`, `Triangles`, `TriangleStripAdjacency`, `TrianglesAdjacency`, `Patches`

### 4.9 BindingType
`Uniform`, `Sampler`, `Buffer`, `BufferBlock`, `Image`, `TextureBuffer`, `Subroutine`

### 4.10 BindingEditor
`Expression`, `Expression2`, `Expression3`, `Expression4`, `Expression2x2`, `Expression2x3`, `Expression2x4`, `Expression3x2`, `Expression3x3`, `Expression3x4`, `Expression4x2`, `Expression4x3`, `Expression4x4`, `Color`

### 4.11 ComparisonFunc
`NoComparisonFunc`, `LessEqual`, `GreaterEqual`, `Less`, `Greater`, `Equal`, `NotEqual`, `Always`, `Never`

### 4.12 BlendEquation
`Add`, `Min`, `Max`, `Subtract`, `ReverseSubtract`

### 4.13 BlendFactor
`Zero`, `One`, `SrcColor`, `OneMinusSrcColor`, `SrcAlpha`, `OneMinusSrcAlpha`, `DstAlpha`, `OneMinusDstAlpha`, `DstColor`, `OneMinusDstColor`, `SrcAlphaSaturate`, `ConstantColor`, `OneMinusConstantColor`, `ConstantAlpha`, `OneMinusConstantAlpha`, `Src1Alpha`

### 4.14 StencilOperation
`Keep`, `Zero`, `Replace`, `Increment`, `IncrementWrap`, `Decrement`, `DecrementWrap`, `Invert`

### 4.15 FrontFace
`CCW`, `CW`

### 4.16 CullMode
`NoCulling`, `Front`, `Back`, `FrontAndBack`

### 4.17 PolygonMode
`Fill`, `Line`, `Point`

### 4.18 LogicOperation
`NoLogicOperation`, `Copy`, `Clear`, `Set`, `CopyInverted`, `NoOp`, `Invert`, `And`, `Nand`, `Or`, `Nor`, `Xor`, `Equiv`, `AndReverse`, `AndInverted`, `OrReverse`, `OrInverted`

### 4.19 Filter (Sampler)

**Minification**: `Nearest`, `Linear`, `NearestMipMapNearest`, `NearestMipMapLinear`, `LinearMipMapNearest`, `LinearMipMapLinear`

**Magnification**: `Nearest`, `Linear`

### 4.20 WrapMode
`Repeat`, `MirroredRepeat`, `ClampToEdge`, `ClampToBorder`

### 4.21 TextureTarget
`Target1D`, `Target1DArray`, `Target2D`, `Target2DArray`, `Target3D`, `TargetCubeMap`, `TargetCubeMapArray`

### 4.22 GeometryType (Ray Tracing)
`AxisAlignedBoundingBoxes`, `Triangles`

### 4.23 ImageBindingFormat

For compute shader image bindings (the `imageFormat` property on `Binding` when `bindingType: "Image"`):

| Format | Description |
|--------|-------------|
| `Internal` | Infer from texture |
| **R-channel** | `r8`, `r8ui`, `r8i`, `r16`, `r16_snorm`, `r16f`, `r16ui`, `r16i`, `r32f`, `r32ui`, `r32i` |
| **RG-channel** | `rg8`, `rg8_snorm`, `rg8ui`, `rg8i`, `rg16`, `rg16_snorm`, `rg16ui`, `rg16i`, `rg16f`, `rg32f`, `rg32ui`, `rg32i` |
| **RGB-channel** | `rgb32f`, `rgb32i`, `rgb32ui`, `r11f_g11f_b10f` |
| **RGBA-channel** | `rgba8`, `rgba8_snorm`, `rgba8ui`, `rgba8i`, `rgb10_a2`, `rgb10_a2ui`, `rgba16`, `rgba16_snorm`, `rgba16f`, `rgba16ui`, `rgba16i`, `rgba32f`, `rgba32i`, `rgba32ui` |

### 4.24 TextureFormat

Uses Qt's `QOpenGLTexture::TextureFormat` enum names. Key formats:

| Category | Formats |
|----------|---------|
| **Color 8-bit** | `R8_UNorm`, `R8_SNorm`, `RG8_UNorm`, `RG8_SNorm`, `RGB8_UNorm`, `RGB8_SNorm`, `RGBA8_UNorm`, `RGBA8_SNorm` |
| **Color 16-bit** | `R16_UNorm`, `R16_SNorm`, `R16F`, `RG16_UNorm`, `RG16_SNorm`, `RG16F`, `RGB16_UNorm`, `RGB16_SNorm`, `RGB16F`, `RGBA16_UNorm`, `RGBA16_SNorm`, `RGBA16F` |
| **Color 32-bit** | `R32F`, `RG32F`, `RGB32F`, `RGBA32F` |
| **Integer** | `R8I`, `R8U`, `R16I`, `R16U`, `R32I`, `R32U`, `RG8I`, `RG8U`, `RG16I`, `RG16U`, `RG32I`, `RG32U`, `RGB8I`, `RGB8U`, `RGB16I`, `RGB16U`, `RGB32I`, `RGB32U`, `RGBA8I`, `RGBA8U`, `RGBA16I`, `RGBA16U`, `RGBA32I`, `RGBA32U` |
| **SRGB** | `SRGB8`, `SRGB8_Alpha8` |
| **Packed** | `R5G6B5`, `RGBA4`, `RGB5A1`, `RGB10A2`, `RGB9E5`, `RG11B10F` |
| **Depth/Stencil** | `D16`, `D32`, `D32F`, `D24S8`, `D32FS8X24`, `S8` |
| **Compressed** | `RGB_DXT1`, `RGBA_DXT1`, `RGBA_DXT3`, `RGBA_DXT5`, `SRGB_DXT1`, `SRGB_Alpha_DXT1`, `SRGB_Alpha_DXT3`, `SRGB_Alpha_DXT5`, `R_ATI1N_UNorm`, `R_ATI1N_SNorm`, `RG_ATI2N_UNorm`, `RG_ATI2N_SNorm`, `RGB_BP_UNorm`, `SRGB_BP_UNorm`, `RGB_BP_SIGNED_FLOAT`, `RGB_BP_UNSIGNED_FLOAT`, `RGB8_ETC1`, `RGB8_ETC2`, `SRGB8_ETC2`, `RGB8_PunchThrough_Alpha1_ETC2`, `SRGB8_PunchThrough_Alpha1_ETC2`, `RGBA8_ETC2_EAC`, `SRGB8_Alpha8_ETC2_EAC`, `R11_EAC_UNorm`, `R11_EAC_SNorm`, `RG11_EAC_UNorm`, `RG11_EAC_SNorm` |

---

## 5. Evaluation Pipeline

### 5.1 Triggering Evaluation

| Shortcut | Mode | Behavior |
|----------|------|----------|
| **F6** | Manual | Evaluate once. Runs items with `ManualEvaluation` and `ResetEvaluation`. |
| **F7** | Automatic | Re-evaluate whenever session changes (shader edit, binding change, etc.) |
| **F8** | Steady | Continuous evaluation every frame (for animations). Runs `EveryEvaluation` items. |

### 5.2 ExecuteOn Logic

```
ResetEvaluation    → executes only on Reset
ManualEvaluation   → executes on Reset or Manual
EveryEvaluation    → executes always (every frame)
```

### 5.3 Call Evaluation Order

1. All active (`checked: true`) calls are collected top-to-bottom.
2. Scripts execute in document order (no scope isolation).
3. Bindings apply to all calls below them in the same scope.
4. Groups with `iterations > 1` repeat their children N times.
5. Elapsed GPU time per call is measured via timer queries and reported in the Message window.

### 5.4 The checked Property

Each Call and Script has a `checked` boolean (checkbox in the UI). Unchecked items are **completely skipped** — they are not added to the command queue. Used items from the last evaluation are highlighted in the session tree.

---

## 6. Scripting API

GPUpad uses **Qt's QJSEngine** for JavaScript evaluation. Scripts run in a dedicated thread with 5-second timeout protection.

### 6.1 `app` — Application Object

**Properties** (read/write unless noted):

| Property | Type | Description |
|----------|------|-------------|
| `frameIndex` | Number | Current animation frame |
| `frameRate` | Number | Playback rate |
| `time` | Number | Elapsed time (seconds) |
| `timeDelta` | Number (r/o) | Delta since last frame |
| `date` | [Number] (r/o) | `[year, month, day, secondsOfDay]` |
| `session` | Session (r/o) | Session manipulation API |
| `mouse` | Mouse (r/o) | Mouse input state |
| `keyboard` | Keyboard (r/o) | Keyboard input state |

**Methods**:

| Method | Returns | Description |
|--------|---------|-------------|
| `openEditor(fileName, title?)` | Editor | Open file in editor/QML panel |
| `openFileDialog(pattern)` | String? | Native file dialog |
| `loadLibrary(fileName)` | Object | Load JS library or C++ DLL |
| `callAction(id, args...)` | any | Invoke another custom action |
| `enumerateFiles(pattern)` | [String] | Glob file listing (supports `**`) |
| `readTextFile(fileName)` | String? | Read text file contents |
| `writeTextFile(fileName, text)` | Bool | Write text file |
| `writeBinaryFile(fileName, data)` | Bool | Write binary file |

### 6.2 `app.session` — Session Object

**Properties**:

| Property | Type | Description |
|----------|------|-------------|
| `name` | String | Session name |
| `items` | [Item] (r/o) | Top-level items |
| `selection` | [Item] (r/o) | Currently selected items |

**Item Query**:

| Method | Returns | Description |
|--------|---------|-------------|
| `findItem(ident, origin?, subItems?)` | Item? | Find first matching item |
| `findItems(ident, origin?, subItems?)` | [Item] | Find all matching items |
| `getParentItem(ident)` | Item? | Get parent of item |

**Item identifiers** (`ident` parameter) accept:
- **String** — Item name or path (`"groupName/itemName"`)
- **Number** — Item ID
- **Function** — Filter callback `(item) => boolean`
- **Object** — Item object with `.id` property

**Item Manipulation**:

| Method | Returns | Description |
|--------|---------|-------------|
| `insertItem(parent?, object)` | Item | Insert new item as last child |
| `insertItemAfter(sibling, object)` | Item | Insert after sibling |
| `insertItemBefore(sibling, object)` | Item | Insert before sibling |
| `replaceItems(parent, [objects])` | void | Replace all children |
| `clearItems(parent)` | void | Remove all children |
| `deleteItem(ident)` | void | Delete item and children |

**Data Operations**:

| Method | Description |
|--------|-------------|
| `setBufferData(ident, data)` | Set buffer contents (auto-updates backing file) |
| `setBlockData(ident, data)` | Set block region within buffer |
| `setTextureData(ident, data)` | Set texture pixel data |
| `setShaderSource(ident, source)` | Replace shader source text |
| `setScriptSource(ident, source)` | Replace script source text |

**Data** can be: JavaScript arrays `[1, 2, 3]`, typed arrays `new Float32Array(...)`, or library arrays from `loadLibrary`.

**GPU Handles**:

| Method | Returns | Description |
|--------|---------|-------------|
| `getTextureHandle(ident)` | Number (uint64) | Bindless texture handle |
| `getBufferHandle(ident)` | Number (uint64) | Buffer device address |

**Shader Processing**:

| Method | Returns | Description |
|--------|---------|-------------|
| `processShader(ident, type)` | String/Data | Process shader |

`type` values: `"preprocess"`, `"glsl"`, `"hlsl"`, `"spirv"`, `"spirvBinary"`, `"ast"`, `"programBinary"`, `"json"`

**Editor Opening**:

| Method | Returns | Description |
|--------|---------|-------------|
| `openEditor(ident)` | Editor? | Open item in editor (e.g. texture in viewport) |

### 6.3 `app.mouse` — Mouse State

| Property | Type | Description |
|----------|------|-------------|
| `coord` | {x, y} | Pixel coordinates (top-left origin) |
| `fragCoord` | {x, y} | Fragment coordinates (bottom-left origin, for shaders) |
| `prevCoord` | {x, y} | Previous frame pixel coordinates |
| `prevFragCoord` | {x, y} | Previous frame fragment coordinates |
| `delta` | {x, y} | Movement since last frame |
| `buttons` | [State] | Button states: `0`=Up, `1`=Down, `2`=Pressed, `-1`=Released |
| `editorSize` | {x, y} | Viewport dimensions |

### 6.4 `app.keyboard` — Keyboard State

| Property | Type | Description |
|----------|------|-------------|
| `keys` | [State] | Key states: `0`=Up, `1`=Down, `2`=Pressed, `-1`=Released |

### 6.5 Editor Object

Returned by `app.openEditor()`:

| Property | Type | Description |
|----------|------|-------------|
| `fileName` | String (r/o) | Editor file path |
| `viewportSize` | [width, height] | Viewport dimensions (reactive) |

### 6.6 `console` — Logging

| Method | Description |
|--------|-------------|
| `console.log(msg, ...)` | Info message |
| `console.warn(msg, ...)` | Warning message |
| `console.error(msg, ...)` | Error message |

Output appears in the **Message** window.

### 6.7 Expression Bindings

Binding values with `editor: "Expression"` (and variants) are evaluated as JavaScript expressions every frame. Available globals:
- All `app.*` properties and methods
- `Math.*` standard library
- Variables defined by earlier scripts in the session
- `target.viewportSize` for viewport-dependent calculations

Examples:
```javascript
"Math.sin(app.time)"
"app.mouse.fragCoord"
"[Math.cos(app.time), Math.sin(app.time), 0]"
"target.viewportSize[0]"
```

---

## 7. Custom Actions

### 7.1 Discovery

Actions live in `extra/actions/`. Each is either:
- A **standalone `.js` file** (simplest form)
- A **directory** containing `script.js` (+ optional `ui.qml`, `module.cpp`, `CMakeLists.txt`)

Actions appear in the **Session** menu automatically.

### 7.2 Manifest

Every action declares a manifest:

```javascript
const manifest = {
  name: "&Action Name...",        // Menu text (& = keyboard accelerator)
  applicable: true                // Boolean or function: () => boolean
}
```

When `applicable` is `false` or returns `false`, the menu item is greyed out.

### 7.3 Action Types

| Type | Components | When to Use |
|------|-----------|-------------|
| **Pure JS** | `ActionName.js` | Simple batch operations (compile shaders, insert items) |
| **JS + QML** | `script.js` + `ui.qml` | Interactive tools with UI panels |
| **JS + QML + C++** | `script.js` + `ui.qml` + `module.cpp` | CPU-heavy operations (mesh generation, file parsing) |
| **QML only** | `ui.qml` with C++ module | Standalone UI components |

### 7.4 JS + QML Communication

```javascript
// script.js
class Script {
  constructor() { /* init */ }
  initializeUi(ui) { /* wire QML controls */ }
  someMethod() { return "value"; }
}
this.script = new Script();
app.openEditor("ui.qml", manifest.name);
```

```qml
// ui.qml — calls back to JS
Button {
  onClicked: script.someMethod()
}
```

QML panels opened via `app.openEditor("file.qml")` share the same script engine, so `script.*` methods are directly callable.

### 7.5 C++ Plugin Pattern

```cpp
// module.cpp
#include "dllreflect.h"

std::string myFunction(const std::string& json) { /* ... */ }

DLLREFLECT_BEGIN()
DLLREFLECT_FUNC(myFunction)
DLLREFLECT_END()
```

```javascript
// script.js
var lib = app.loadLibrary("ModuleName");
var result = lib.myFunction(JSON.stringify(params));
```

C++ plugins compile as shared libraries. Functions receive/return strings, numbers, and arrays. They do **not** have GPU context access.

### 7.6 Action Composition

Actions can invoke other actions:
```javascript
app.callAction("GenerateMesh", { type: "Sphere", slices: 32 });
```

---

## 8. Built-in Shader Features

### 8.1 Printf Debugging

GPUpad automatically injects a `printf()` function into shaders. Use with a preprocessor guard:

```glsl
#if defined(GPUPAD)
  if (gl_FragCoord.xy == uMouseFragCoord)
    printf("value = %f, color = %v4f", myValue, myColor);
#endif
```

**How it works**:
1. Source is scanned for `printf()` calls.
2. Each call is rewritten to `_printfBegin(formatIdx, argCount), _printf(arg1), ...`
3. An SSBO (`_printfBuffer`) with atomic operations collects output.
4. After rendering, the buffer is read back and formatted messages appear in the **Message** window.

**Supported format specifiers**: `%d`, `%u`, `%f`, `%e`, `%v2f`, `%v3f`, `%v4f`, `%v2d`, etc. (vector types use `v<N><type>` syntax).

### 8.2 Predefined Macros

| Macro | Value | Description |
|-------|-------|-------------|
| `GPUPAD` | `1` | Always defined when compiling in GPUpad |
| `GPUPAD_OPENGL` | `1` | When using OpenGL renderer |
| `GPUPAD_GLSLANG` | `1` | When using glslang compiler |
| `GPUPAD_D3DCOMPILER` | `1` | When using D3DCompiler |
| `GPUPAD_DXC` | `1` | When using DXC compiler |
| `GPUPAD_SLANG` | `1` | When using Slang compiler |

### 8.3 Shader Preamble

The session-level `shaderPreamble` string is prepended to every shader. Per-shader `preamble` strings are also supported. Common uses:
- Version declarations: `#version 460`
- Feature defines: `#define PASS 0`
- Extension enables: `#extension GL_EXT_nonuniform_qualifier : enable`

### 8.4 Includable Shaders

Shaders with `shaderType: "Includable"` are not compiled as pipeline stages. They serve as shared include files that can be `#include`d by other shaders in the same program.

---

## 9. Sample Sessions Catalogue

All samples live in `extra/samples/GLSL/`. They can be opened from the **Help** menu and serve as templates (Save As copies all dependencies).

### 9.1 Atomic Counters

**Purpose**: Atomic operations on counters using SSBO with atomic functions.

| Feature | Detail |
|---------|--------|
| Calls | `ClearBuffer`, `Compute` |
| Bindings | `Buffer`, `TextureBuffer` |
| Format | `rgba8ui` (texture buffer) |
| Compute | 16×1×1 work groups |

Demonstrates: atomic counter buffers, SSBO atomic operations, TextureBuffer bindings.

### 9.2 Bindless Texture

**Purpose**: Dynamic texture access without traditional binding points using uint64 GPU handles.

| Feature | Detail |
|---------|--------|
| Calls | `ClearTexture`, `Draw` |
| Bindings | `BufferBlock` (Uint64 handles) |
| Script | `setHandles.js` — populates handles via `getTextureHandle()` |

Demonstrates: bindless texture handles, BufferBlock bindings, script-driven GPU handle management.

### 9.3 Bitonic Sort

**Purpose**: Parallel bitonic sorting of 262,144 elements using compute shaders.

| Feature | Detail |
|---------|--------|
| Calls | `Compute` (FillRandom) |
| Buffer | 262,144 rows of Uint32 |
| Script | Dynamic multi-pass compute call insertion |

Demonstrates: large-scale compute, dynamic session manipulation from scripts, expression-based work groups.

### 9.4 Buffer Reference

**Purpose**: Vulkan buffer device addresses for pointer-like GPU memory access.

| Feature | Detail |
|---------|--------|
| Renderer | **Vulkan** |
| Bindings | `Buffer` with Uint64 device addresses |
| Script | `setHandles.js` — populates via `getBufferHandle()` |

Demonstrates: Vulkan-specific buffer references, push constants, device address pointers.

### 9.5 Compute (Game of Life)

**Purpose**: Conway's Game of Life using compute shaders with ping-pong texture swapping.

| Feature | Detail |
|---------|--------|
| Calls | `ClearTexture`, `Compute`, `SwapTextures` |
| Bindings | `Image` (read + write) |
| Textures | Two textures for ping-pong |

Demonstrates: compute shaders, image load/store, ping-pong pattern via `SwapTextures`.

### 9.6 Cube

**Purpose**: Classic 3D textured cube with depth testing and matrix transforms.

| Feature | Detail |
|---------|--------|
| Calls | `ClearTexture` (color + depth), `Draw` |
| Target | Depth: `D16`, Color: `SRGB8_Alpha8` |
| Stream | Position, Normal, TexCoord attributes |
| Bindings | Model/View/Projection matrices, Sampler |

Demonstrates: standard 3D pipeline, depth testing, SRGB rendering, anisotropic filtering, vertex streams.

### 9.7 Custom Actions

**Purpose**: Interactive 3D model viewer showcasing custom action integration.

| Feature | Detail |
|---------|--------|
| Scripts | `ManualEvaluation` + `ResetEvaluation` |
| Actions | `app.callAction()` for GenerateMesh/ImportOBJ |

Demonstrates: manual script execution, orbit camera, action composition.

### 9.8 glTF Viewer

**Purpose**: PBR (Physically Based Rendering) viewer with IBL (Image-Based Lighting).

| Feature | Detail |
|---------|--------|
| Textures | 11 (cubemaps, albedo, normal, metalRoughness, AO, emissive) |
| Target | MSAA (`samples: 4`) |
| Calls | `DrawIndexed` |
| Groups | Nested: DamagedHelmet, Environment, Matrices, Material, Lights |
| Bindings | 25+ (uniforms, samplers, array uniforms) |

Demonstrates: indexed drawing, MSAA, cubemap textures, nested groups, extensive uniform arrays, PBR/IBL pipeline.

### 9.9 Indirect

**Purpose**: GPU-generated draw commands with indirect rendering.

| Feature | Detail |
|---------|--------|
| Calls | `Compute`, `ComputeIndirect`, `ClearTexture`, `DrawIndirect` |
| Buffer | ComputeCommand + DrawCommand blocks at different offsets |
| Target | `polygonMode: "Line"` |

Demonstrates: indirect compute dispatch, indirect drawing, multi-block buffers with offsets, wireframe rendering.

### 9.10 Instancing

**Purpose**: GPU instancing with per-instance attributes.

| Feature | Detail |
|---------|--------|
| Calls | `ClearTexture`, `Draw` |
| Stream | Attributes with `divisor: 0` (per-vertex) and `divisor: 1` (per-instance) |
| Target | Alpha blending (`SrcAlpha`, `OneMinusSrcAlpha`) |
| Call | `instanceCount: 4` |

Demonstrates: instanced rendering, per-instance attributes via divisor, alpha blending.

### 9.11 Javascript Library (Delaunay)

**Purpose**: JavaScript-driven Delaunay triangulation using an external library.

| Feature | Detail |
|---------|--------|
| Scripts | `delaunay.js` (library), `generate.js` (mesh generation) |
| Calls | `DrawIndexed` |
| Buffer | Vertices (Float×2) + Indices (Uint16) |
| Target | `polygonMode: "Line"` |

Demonstrates: `app.loadLibrary()`, JS mesh generation, indexed wireframe drawing, script-driven buffer population.

### 9.12 Mesh Shader

**Purpose**: NVIDIA mesh shader extension for GPU-driven vertex generation.

| Feature | Detail |
|---------|--------|
| Calls | `DrawMeshTasks` (1×1×1 work groups) |
| Program | Mesh + Fragment shaders |
| Extension | `GL_NV_mesh_shader` |

Demonstrates: mesh task dispatch, GPU-side vertex/primitive generation without vertex streams.

### 9.13 Nonuniform Indexing

**Purpose**: Descriptor indexing with dynamic texture array access.

| Feature | Detail |
|---------|--------|
| Renderer | **Vulkan** |
| Bindings | `Sampler` array (`uTexture[0]`, `uTexture[1]`) |
| Extension | `GL_EXT_nonuniform_qualifier` |

Demonstrates: nonuniform descriptor indexing, dynamic texture selection in shaders.

### 9.14 Order-Independent Transparency (OIT)

**Purpose**: Linked-list based order-independent transparency.

| Feature | Detail |
|---------|--------|
| Calls | `ClearBuffer`, `ClearTexture`, `Draw` (2 passes) |
| Textures | R32U heads pointer texture |
| Buffer | Fragment linked-list SSBO |
| Bindings | `Image`, `Buffer` |
| Groups | Organized (Targets, Buffers, Programs, Bindings, Calls) |
| Stream | Instanced (`divisor: 1`, 100 particles) |

Demonstrates: atomic image operations, SSBO linked lists, multi-pass OIT, instanced rendering, group organization.

### 9.15 Paint

**Purpose**: Interactive painting application with mouse input.

| Feature | Detail |
|---------|--------|
| Calls | `ClearTexture` (ResetEvaluation), `Draw` (EveryEvaluation) |
| Bindings | `app.mouse.coord`, `app.mouse.prevCoord`, `app.mouse.buttons[0]` |
| Target | Alpha blending |

Demonstrates: real-time mouse input, persistent render target (clear only on reset), brush rendering.

### 9.16 Particles

**Purpose**: GPU-accelerated particle system with compute + rasterization.

| Feature | Detail |
|---------|--------|
| Programs | Reset (Compute), Update (Compute), Render (VS+FS) |
| Shaders | `Includable` shared particle definition |
| Calls | `Compute` (reset + update), `ClearTexture`, `Draw` (points) |
| Bindings | `Expression2` for animation, `Buffer` for particle data |
| Target | Additive blending (`SrcAlpha` + `One`) |

Demonstrates: multi-stage compute pipeline, includable shader modules, expression-based work groups, additive blending, point rendering.

### 9.17 Printf

**Purpose**: Per-pixel shader printf debugging.

| Feature | Detail |
|---------|--------|
| Binding | `uMouseFragCoord: "app.mouse.fragCoord"` |
| Shader | `#if defined(GPUPAD)` + `printf()` |

Demonstrates: built-in printf debugging, mouse fragment coordinate tracking, conditional per-pixel output.

### 9.18 Quad

**Purpose**: Basic textured quad rendering.

| Feature | Detail |
|---------|--------|
| Calls | `ClearTexture`, `Draw` |
| Bindings | `Sampler` (Nearest filter, Repeat wrap), `Uniform` (color) |
| No Stream | Attributeless fullscreen quad (4 vertices) |

Demonstrates: minimal texture sampling setup, attributeless rendering.

### 9.19 Ray Tracing In Vulkan (Procedural)

**Purpose**: Hardware ray tracing with procedural AABB sphere geometry.

| Feature | Detail |
|---------|--------|
| Renderer | **Vulkan** |
| Calls | `TraceRays` |
| Program | RayGeneration + RayMiss + RayClosestHit + RayIntersection |
| AccelerationStructure | AABB geometry type |
| Buffers | Spheres, Materials, AABBs |
| Script | `generateSpheres.js`, `camera.js` |

Demonstrates: procedural ray intersection, path tracing, depth of field, accumulation buffer, AABB acceleration structure.

### 9.20 Ray Tracing In Vulkan 2 (Mesh)

**Purpose**: Hardware ray tracing with triangle mesh geometry.

| Feature | Detail |
|---------|--------|
| Renderer | **Vulkan** |
| Calls | `TraceRays` |
| Program | RayGeneration + RayMiss + RayClosestHit |
| AccelerationStructure | Triangle geometry type |
| Bindings | `BufferBlock` arrays, `Sampler` with anisotropic |
| Script | `generateScene.js`, `camera.js` |

Demonstrates: triangle mesh acceleration structure, texture mapping in ray tracing, buffer block arrays, dynamic scene generation.

### 9.21 Shadertoy

**Purpose**: Basic Shadertoy-compatible shader rendering.

| Feature | Detail |
|---------|--------|
| Script | Per-frame uniform updates |
| Bindings | `iResolution`, `iFrame`, `iChannel0-3` (Samplers) |
| Calls | `Draw` (TriangleStrip, 4 vertices) |

Demonstrates: Shadertoy uniform conventions, multi-channel sampler bindings, script-driven updates.

### 9.22 Shadertoy 2

**Purpose**: Advanced multi-pass Shadertoy with buffer feedback and keyboard input.

| Feature | Detail |
|---------|--------|
| Calls | `ClearTexture`, `Draw`, `CopyTexture` |
| Textures | RGBA32F buffers, RGBA16F cubemap, R8_UNorm keyboard texture |
| Groups | Nested buffer passes |
| Shader | `preamble: "#define PASS 0"` for multi-pass selection |

Demonstrates: multi-pass feedback loops, `CopyTexture`, keyboard texture input, shader preambles, cubemap sampling.

### 9.23 Sliders

**Purpose**: Interactive parameter control via expression bindings.

| Feature | Detail |
|---------|--------|
| Bindings | `Expression`, `Expression3` editors |
| Values | `app.frameIndex`, `app.time`, `[0.283, 0.511, 0.61]` |

Demonstrates: live expression evaluation, multi-component expression editors, the Sliders custom action companion.

### 9.24 Stencil Buffer

**Purpose**: Two-pass stencil mask + fill rendering.

| Feature | Detail |
|---------|--------|
| Textures | `RGBA8_UNorm` (color), `D24S8` (depth-stencil) |
| Targets | 2 (mask generation + masked fill) |
| Programs | 2 (mask writer + fill renderer) |
| Stencil | Front: `Always`→`Increment` (write), `NotEqual`→reference=0 (read) |

Demonstrates: stencil operations (read/write masks, comparison, increment), multi-pass rendering, D24S8 format.

### 9.25 Subroutine

**Purpose**: GLSL subroutine for runtime shader function selection.

| Feature | Detail |
|---------|--------|
| Binding | `bindingType: "Subroutine"`, `subroutine: "fade1"` |

Demonstrates: `Subroutine` binding type, polymorphic shader functions without recompilation.

### 9.26 Sync Test

**Purpose**: GPU synchronization and timing verification.

| Feature | Detail |
|---------|--------|
| Bindings | Manual resolution (`[1920, 1080]`), `app.frameIndex` |

Demonstrates: fixed-resolution rendering independent of viewport, frame indexing.

### 9.27 Tessellation

**Purpose**: Full GPU tessellation pipeline with control, evaluation, and geometry stages.

| Feature | Detail |
|---------|--------|
| Program | Vertex + TessControl + TessEvaluation + Geometry + Fragment |
| Calls | `DrawIndexed` with `primitiveType: "Patches"` |
| Call | `patchVertices: "3"` |
| Buffer | Binary mesh files (`geodesic_positions.bin`, `geodesic_indices.bin`) |
| Bindings | `TessLevelInner: 5`, `TessLevelOuter: 4`, matrix math expressions |

Demonstrates: complete tessellation pipeline, patch primitives, binary buffer files, complex JS matrix expressions.

### 9.28 Uniforms

**Purpose**: Comprehensive test of all uniform binding patterns.

| Feature | Detail |
|---------|--------|
| Call | `Compute` (1×1×1) |
| Groups | 4 (whole uniforms, elements, blocks, block arrays) |
| Editors | `Expression`, `Expression2`, `Expression2x2`, `Expression2x3`, `Expression3`, `Expression3x3`, `Color` |
| Paths | `u_values_1[0].y[0].z`, `u_block.values_1[1].y[1].w` |

Demonstrates: every binding editor type, nested uniform paths, array-of-blocks indexing, uniform block members.

### 9.29 Video

**Purpose**: Video file playback as a texture source.

| Feature | Detail |
|---------|--------|
| Texture | `fileName: "robin.mp4"` |
| Sampler | `MirroredRepeat` wrap, `LinearMipMapLinear` minification |
| Binding | `uTime: "(Date.now() % 100000) / 1000"` |

Demonstrates: video texture source, mirrored repeat wrapping, JS-computed time uniform.

### 9.30 Volume

**Purpose**: 3D texture manipulation via compute shader.

| Feature | Detail |
|---------|--------|
| Texture | `target: "Target3D"`, `depth: "64"`, `format: "R8_UNorm"` |
| Binding | `Image` with `layer: -1` (all layers) |
| Call | `Compute` with 8×8×64 work groups |

Demonstrates: 3D texture target, volume compute dispatch, all-layer image binding.

---

## 10. Existing Custom Actions Catalogue

Actions in `extra/actions/`:

### 10.1 Compile all shader files to Spir-V

**Type**: Pure JS | **File**: `Compile all shader files to Spir-V.js`

Iterates all Shader items, calls `session.processShader(shader, "spirvBinary")`, writes `.spv` files.

**API used**: `findItems()`, `processShader()`, `writeBinaryFile()`

### 10.2 GenerateMesh

**Type**: C++ + JS + QML | **Directory**: `GenerateMesh/`

Procedural mesh generator (16 types: cube, cylinder, cone, torus, sphere variants, klein bottle, trefoil knot, hemisphere, plane, polyhedra, rock). C++ uses `par_shapes` library.

**API used**: `loadLibrary()`, `insertItem()`, `setBlockData()`, `replaceItems()`, `deleteItem()`

### 10.3 ImportOBJ

**Type**: C++ + JS + QML | **Directory**: `ImportOBJ/`

Wavefront OBJ importer with vertex deduplication, normal generation, transformations (normalize, center, swap Y/Z). C++ uses `rapidobj`.

**API used**: `loadLibrary()`, `openFileDialog()`, `insertItem()`, `setBlockData()`, `replaceItems()`

### 10.4 Import glTF

**Type**: Pure JS | **File**: `Import_glTF.js` | **Status**: Disabled (`applicable: false`)

Parses glTF 2.0 JSON, maps accessors to Buffers/Streams, extracts textures/materials.

**API used**: `openFileDialog()`, `readTextFile()`, `insertItem()`, `findItem()`

### 10.5 Insert Orbit Camera

**Type**: Pure JS | **File**: `Insert Orbit Camera.js`

Creates an interactive orbit camera controller (left-drag rotate, right-drag zoom). Inserts a Script item with embedded camera code + view matrix Binding.

**API used**: `loadLibrary()` (gl-matrix), `insertItem()`, `setScriptSource()`

### 10.6 Inspector

**Type**: JS + QML + GLSL | **Directory**: `Inspector/`

Real-time GLSL expression debugger. Rewrites fragment shaders to visualize any expression. GPU-resident pipeline: expression render → histogram compute → composite display with mapping modes (Linear/Sigmoid/Log), channel toggles, OOR highlighting, per-pixel printf inspection.

**API used**: `findItems()`, `insertItem()`, `setShaderSource()`, `readTextFile()`, `deleteItem()`, `getParentItem()`, `openEditor()`

### 10.7 NodeGraph

**Type**: QML + C++ module | **Directory**: `NodeGraph/`

Visual node-graph editor with drag-and-drop nodes, attribute connections, and procedural graph layout. Uses a custom C++ `NodeGraph 1.0` QML module.

**API used**: `openEditor()`

### 10.8 Sliders

**Type**: JS + QML | **Directory**: `Sliders/`

Auto-generates slider controls for all Binding uniform values in the session.

**API used**: `findItems()`, `findItem()`, `openEditor()`

### 10.9 Timer

**Type**: JS + QML | **Directory**: `Timer/`

Displays `app.time`, `app.timeDelta`, `app.frameRate`, and `app.date` in a dockable panel with 16ms polling.

**API used**: `app.frameRate`, `app.time`, `app.timeDelta`, `app.date`, `openEditor()`

---

## Appendix A — Quick Recipes

### Fullscreen Fragment Shader (Attributeless Quad)

```json
{
  "type": "Call",
  "callType": "Draw",
  "primitiveType": "TriangleStrip",
  "count": "4"
}
```
Vertex shader generates positions from `gl_VertexID`. No Stream needed.

### Ping-Pong Texture Pattern

```json
[
  { "type": "Call", "callType": "Compute", "programId": ..., ... },
  { "type": "Call", "callType": "SwapTextures", "textureId": ..., "fromTextureId": ... }
]
```

### Conditional Reset/Per-Frame Execution

```json
{ "type": "Call", "executeOn": "ResetEvaluation", "callType": "ClearTexture", ... },
{ "type": "Call", "executeOn": "EveryEvaluation", "callType": "Draw", ... }
```

### GPU Histogram via Compute + SSBO

1. `Buffer` with `Block` (histogram bins as Uint32 fields)
2. `ClearBuffer` call (ResetEvaluation or EveryEvaluation)
3. `Compute` call reading a texture via `Image` binding, writing to `Buffer` binding with atomics

### Per-Pixel Debug Output

```glsl
#if defined(GPUPAD)
uniform vec2 uMouseFragCoord;
if (ivec2(gl_FragCoord.xy) == ivec2(uMouseFragCoord))
    printf("val=%f", myValue);
#endif
```

Bind `uMouseFragCoord` with `editor: "Expression"`, `values: ["app.mouse.fragCoord"]`.

---

*Generated from source analysis of the GPUpad codebase (`houmain/gpupad`) and all sample sessions/actions.*
