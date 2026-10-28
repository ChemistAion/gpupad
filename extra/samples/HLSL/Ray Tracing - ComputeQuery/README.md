# Ray Tracing - ComputeQuery

Derived from the HLSL Ray Tracing sample: trefoil mesh, vertex layout and camera.
One diffuse hit, sky background, no accumulation or external texture.
Open raytracing.gpjs, reset evaluation and inspect OutputImage. Move the mouse
to orbit; use steady evaluation for animation. Mesh generation uses GPUpad's
bundled GenerateMesh library directly.

Requires Direct3D compute acceleration-structure binding and DXC SM6.5.
The compute shader uses inline triangle traversal and committed hit data.
There is no ray-generation pipeline or hit/miss shader callback.
