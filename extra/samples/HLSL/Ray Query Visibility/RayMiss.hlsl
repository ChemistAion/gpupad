#include "SphereVisibility.hlsl"
#include "PipelineDiagnostics.hlsl"

[shader("miss")]
void RayMiss(inout VisibilityPayload payload)
{
    const uint index = DispatchRaysIndex().y * 6u + DispatchRaysIndex().x;
    PipelineDiagnostics[index].Outcome.x = 1u;
    payload.Blocked = 0u;
    DeviceMemoryBarrier();
}
