#include "SphereVisibility.hlsl"

[shader("closesthit")]
void RayClosestHit(inout VisibilityPayload payload, in SphereAttributes attributes)
{
    // Deliberately empty; the trace retains SKIP_CLOSEST_HIT_SHADER.
}
