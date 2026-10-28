# Ray Tracing - Lifecycle

Derived from the HLSL Ray Tracing sample: trefoil mesh, vertex layout and camera.
One diffuse hit, sky background, no accumulation or external texture.
Open raytracing.gpjs, reset evaluation and inspect OutputImage. Move the mouse
to orbit; use steady evaluation for animation. Mesh generation uses GPUpad's
bundled GenerateMesh library directly.

Requires shared instances, explicit shader records/local constants and TLAS/BLAS
lifecycle support. Three hit records supply colors through one shared local-root
schema; all instances deliberately use ID0. Edit mode in generateScene.js and
reset evaluation: 0 static records, 1 animated TLAS rebuild, 2 animated TLAS
update, 3 GPU-deformed BLAS rebuild, 4 GPU-deformed BLAS update (default).
Modes3/4 deform the rest mesh on the GPU with matching normal transformation;
layout, indices and primitive activity remain unchanged. This demonstrates
visible behavior; the image alone does not establish rebuild/update execution.
