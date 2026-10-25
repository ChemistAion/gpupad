#include "SphereVisibility.hlsl"

[shader("miss")]
void RayMiss(inout VisibilityPayload payload)
{
    payload.Blocked = 0u;
}
