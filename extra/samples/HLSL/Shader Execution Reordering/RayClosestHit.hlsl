#include "RayPayload.hlsl"

[shader("closesthit")]
void RayClosestHit(inout RayPayload payload,
    BuiltInTriangleIntersectionAttributes attributes)
{
    payload.Hit = 1;
    payload.Invoked = 0x12340001;
}
