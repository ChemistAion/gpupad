// GPUpad requires a Miss export to launch raygen; inline traversal never calls it.
// This formal parameter is unused scaffolding; raygen transmits no pipeline payload.
struct UnusedPayload { uint Unused; };

[shader("miss")]
void RayMiss(inout UnusedPayload payload)
{
}
