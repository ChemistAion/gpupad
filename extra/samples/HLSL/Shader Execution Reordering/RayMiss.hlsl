#include "RayPayload.hlsl"

[shader("miss")]
void RayMiss(inout RayPayload payload)
{
    payload.Hit = 0;
    payload.Invoked = 0x56780000;
}
