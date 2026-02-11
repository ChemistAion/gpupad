---
name: gpupad
description: Use this skill when creating or editing GPUpad session (.gpjs) items and scripting based on README Items/Scripting and extra/samples.
---

# GPUpad sessions and scripting

## When to use
- Creating or modifying GPUpad session files (*.gpjs) or the items inside them.
- Adding or updating GPUpad scripts that drive session data or automation.

## Session file shape
- A session file is JSON and may be either:
  - An array of items (many samples like `extra\samples\Compute\compute.gpjs`), or
  - A `Session` object with an `items` array (see `extra\samples\Uniforms\Uniforms.gpjs`, `extra\samples\Ray Tracing In Vulkan\raytracing.gpjs`).
- Every item has a numeric `id` and a `type`. References use `*Id` fields (e.g., `programId`, `targetId`, `textureId`, `bufferId`, `blockId`, `fieldId`).
- Many numeric properties are stored as strings to allow expressions; preserve this pattern (see `Uniforms.gpjs` and `compute.gpjs`).

## Item catalog (README + samples)
Use these item types and their common relationships:

- Session: top-level settings and `items`.
  - Fields seen in samples: `renderer`, `shaderCompiler`, `shaderIncludePaths`, `shaderPreamble`, `spirvVersion`, `autoMapBindings`, `autoMapLocations`, `autoSampledTextures`, `flipViewport`, `reverseCulling`, `vulkanRulesRelaxed`.
- Group: used for scoping and structuring.
  - Fields: `inlineScope` (true means no new scope), `dynamic`, `iterations`, `items`.
- Program -> Shader: shader programs used by calls.
  - Shader fields: `fileName`, `shaderType`, `language`, `entryPoint`, `preamble`.
  - Shader types in samples: Vertex, Fragment, Geometry, TessControl, TessEvaluation, Compute, Mesh, RayGeneration, RayMiss, RayClosestHit, RayIntersection, Includable.
- Call: execution units evaluated in order.
  - Call types in samples: Draw, DrawIndexed, DrawIndirect, DrawMeshTasks, Compute, ComputeIndirect, TraceRays, ClearTexture, ClearBuffer, CopyTexture, SwapTextures.
  - Common fields: `programId`, `targetId`, `vertexStreamId`, `workGroupsX/Y/Z`, `count`, `first`, `instanceCount`, `primitiveType`, `indexBufferBlockId`, `indirectBufferBlockId`, `accelerationStructureId`.
- Texture: color/depth/stencil images.
  - Fields: `target` (Target2D/Target3D/TargetCubeMap), `format`, `width/height/depth`, `samples`, `fileName`, `flipVertically`.
- Target -> Attachment: render targets and state.
  - Target fields: `polygonMode`, `cullMode`, `frontFace`, `logicOperation`, `blendConstant`, `defaultWidth/Height/Layers/Samples`, `items`.
  - Attachment fields: `textureId`, `level`, blend settings, depth/stencil state.
- Binding: connects data to shader bindings.
  - Binding types in samples: Uniform, Buffer, BufferBlock, Image, Sampler, TextureBuffer, Subroutine.
  - Fields: `name` (must match shader binding name), `values`, `bufferId`, `blockId`, `textureId`, `imageFormat`, `layer`, `level`, sampler settings.
  - Uniform bindings often use `editor` (e.g., `Expression`, `Expression4`, `Expression2x2`).
- Buffer -> Block -> Field: structured binary buffers.
  - Block fields: `rowCount`, `offset`, `items`.
  - Field fields: `dataType`, `count`, `padding`.
  - Example: `extra\samples\Javascript Library\delaunay.gpjs`.
- Stream -> Attribute: vertex input bindings.
  - Attribute fields: `fieldId`, `divisor`, `normalize`.
- Script: JavaScript files run during evaluation.
  - Fields: `fileName`, `executeOn` (ResetEvaluation or EveryEvaluation).
- AccelerationStructure -> Instance -> Geometry: ray tracing data.
  - Geometry fields: `geometryType`, `vertexBufferBlockId`, `offset`, `count`.
  - Instance fields: `transform`, `items`.
  - Example: `extra\samples\Ray Tracing In Vulkan\raytracing.gpjs`.

## Scripting patterns (from samples)
Use the app and session APIs listed in README "Scripting", and follow these patterns:

- Query/update items:
  - `app.session.findItem("Buffer/Vertices")`, `app.session.insertItem(parent, { ... })`, `app.session.clearItems(group)`.
  - Example: `extra\samples\Bitonic sort\Script.js` builds a dynamic group and injects `Binding` + `Call` items.
- Populate buffers and textures:
  - `app.session.setBlockData("Buffer/Vertices", data)` and `app.session.setTextureData("Textures/Keyboard", data)` (see `extra\samples\Javascript Library\generate.js`, `extra\samples\Shadertoy 2\keyboard.js`).
- Input-driven updates:
  - Mouse: `app.mouse.coord`, `app.mouse.delta` (see `extra\samples\Ray Tracing In Vulkan\camera.js`).
  - Keyboard: `app.keyboard.keys` (see `extra\samples\Shadertoy 2\keyboard.js`).
- External libraries and actions:
  - `app.loadLibrary("gl-matrix.js")` (see `extra\samples\Custom Actions\script.js`).
  - `app.callAction("ImportOBJ", {...})` / `app.callAction("GenerateMesh", {...})` (see `extra\samples\Custom Actions\script.js`).

## Editing guidance
- Keep `id` values unique and update all `*Id` references when adding/removing items.
- Preserve string expressions for numeric values when samples do (work group sizes, offsets, counts).
- Respect Group scoping rules: bindings inside a group only affect later calls within the same scope unless `inlineScope` is true.
- Keep asset paths relative to the session file and match sample patterns.
